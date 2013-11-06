#include <lwk/gdbio.h>
#include <lwk/waitq.h>
#include <lwk/htable.h>
#include <lwk/list.h>
#include <lwk/spinlock.h>
#include <lwk/driver.h>

#include <arch/atomic.h>
#include <arch-generic/fcntl.h>

#define dbg(fmt,args...)
struct gdbio_state{
	char gdbcons[32]; //gdb console id 
	struct hlist_node ht_link;
	int detached;
};

static struct htable *gdbio_htable = NULL;
static spinlock_t htable_lock;

static uint64_t gdbio_hash_key(const void *gdbcons, size_t order){
	char *buf = (char *) gdbcons;
	int sum = 0, i;
	for(i=0; i<strlen(buf); i++){
		sum += buf[i];
	}
	//printk("GDB: hash(%s)=%d\n", (char *) gdbcons, sum % (1<<order));
	return (sum % (1<<order));   
}

static int gdbio_key_compare(const void *key_in_search, 
	const void *key_in_table){
	char *search = (char *) key_in_search;
	char *table = (char *) key_in_table;
	return strcmp(search, table); 
}

static void gdbio_htable_add(struct gdbio_state *gs){
	spin_lock(&htable_lock); 
	htable_add(gdbio_htable, gs);
	spin_unlock(&htable_lock);
}

static void gdbio_htable_del(struct gdbio_state *gs){
	spin_lock(&htable_lock); 
	htable_del(gdbio_htable, gs);
	spin_unlock(&htable_lock);
}

static struct gdbio_state *gdbio_htable_lookup(char *gdbcons){
	spin_lock(&htable_lock); 
	struct gdbio_state *gs = htable_lookup(gdbio_htable, gdbcons);
	spin_unlock(&htable_lock);
	return gs;
}
/*
void gdbio_htable_test(void){
	char * cons = "gdbcons0";
	struct gdbio_state *gs = kmem_alloc(sizeof(struct gdbio_state));
	strcpy(gs->gdbcons, cons);

	gdbio_htable_add(gs);

	if(gdbio_htable_lookup(gs->gdbcons)){
		printk("GDB: gdbio_htable test succeed\n");
	}else{
		printk("GDB: gdbio_htable test failed\n"); 
	}     
	kmem_free(gs);
}
*/
static int __init gdbio_module_init(void){     
	gdbio_htable = htable_create(
		GDB_HTABLE_ORDER,
		offsetof(struct gdbio_state, gdbcons),
		offsetof(struct gdbio_state, ht_link),
		gdbio_hash_key,
		gdbio_key_compare
	);
	if(!gdbio_htable){
		printk("GDB Error: init gdbio module failed\n");
		return -1;
	}
	spin_lock_init(&htable_lock); 
	//printk("GDB: init gdbio module successfully\n");

	return 0;
}

static void __exit gdbio_module_deinit(void){
	//printk("GDB: deinit gdbio module\n");
	spin_lock(&htable_lock);
	if(gdbio_htable){
		struct gdbio_state *gs;
		struct htable_iter iter = htable_iter( gdbio_htable );
		while( (gs = htable_next( &iter )) ){
			kmem_free(gs);    
		}
		 
		htable_destroy(gdbio_htable);
		gdbio_htable = NULL; 
	}
	spin_unlock(&htable_lock); 
}



struct file *gdbio_connect(char *gdbcons){//invoked by gdbclient
	struct gdbio_state *gs = gdbio_htable_lookup(gdbcons);
	if(gs){
		printk("GDB Error: %s is busy\n", gdbcons);
		return NULL;    
	}

	struct file *fd = open_gdb_fifo(gdbcons);
	if(fd){
		gs = kmem_alloc(sizeof(struct gdbio_state));
		strcpy(gs->gdbcons, fd->inode->name);
		gs->detached = 0;

		gdbio_htable_add(gs);        

		//printk("GDB: gdb client connect to gdb server stub via %s\n", (char *) gs->gdbcons); 
		//gdbclient_write(fd, "+", 1);
	}
	return fd;        
}

void gdbio_detach(struct file *filep){//invoked by gdbserver
	struct gdbio_state *gs = gdbio_htable_lookup(filep->inode->name); 
	/*if(!gs){
		return;     
	}*/
	gs->detached = 1;    

	//wakeup gdb client (gdb client may block in gdbclient_read)
	struct gdb_fifo_file *file = (struct gdb_fifo_file *) filep->private_data;
	struct gdbio_buffer* out_buf = file->out_buffer;
	strcpy(out_buf->buf, "$#00");
	atomic_set(&out_buf->avail_data, 4);
	waitq_wake_nr(&out_buf->poll_wait, 1);

	kfs_close(filep); 
}

static int gdbio_is_detached(struct file *filep){ 
	if(!filep){
		return 1;    
	}
	struct gdbio_state *gs = gdbio_htable_lookup(filep->inode->name);
	if(gs && gs->detached){
		//printk("GDB Error: gdb server stub is not running\n");
		gdbio_htable_del(gs);
		kmem_free(gs);
		rm_gdb_fifo(filep);  
		return 1;    
	}    
	return 0;
}

static struct gdbio_buffer* alloc_gdbio_buffer( int length ) {
	struct gdbio_buffer* pbuf = kmem_alloc(sizeof(struct gdbio_buffer));

	if ( ! pbuf ) return NULL;

	if ( ( pbuf->buf = kmem_alloc( length ) ) == NULL ) {
		kmem_free(pbuf);       
		return NULL;
	}
	memset(pbuf->buf, 0, sizeof(pbuf->buf));

	waitq_init(&pbuf->poll_wait);

	atomic_set(&pbuf->avail_data, 0);
	atomic_set(&pbuf->avail_space, length);

	pbuf->rindex = 0;
	pbuf->windex = 0;
	pbuf->capacity = length;
	return pbuf;
}

static void free_gdbio_buffer( struct gdbio_buffer* buf ) {
	kmem_free( buf->buf );	
	kmem_free( buf );
}

/*static void reset_gdbio_buffer(struct gdbio_buffer* pbuf){
	memset(pbuf->buf, 0, sizeof(pbuf->buf));
	pbuf->rindex = 0;
	pbuf->windex = 0;
	atomic_set(&pbuf->avail_data, 0);
	atomic_set(&pbuf->avail_space, pbuf->capacity);
	waitq_init(&pbuf->poll_wait);    //TODO ?? 
}*/

#if 0
static void buf_print(char *buffer, ssize_t len){
	int i = 0;
	while(i < len){
		printk("%c", buffer[i++]);        
	}
	printk("\n");
}
#endif

static ssize_t buf_read(struct gdbio_buffer *pbuf, char *buffer, ssize_t len){
	wait_event_interruptible(pbuf->poll_wait, atomic_read(&pbuf->avail_data));

	ssize_t avail_data = atomic_read(&pbuf->avail_data);
	len = (len > avail_data ? avail_data : len);
	int i = 0;
	while(i < len){
		buffer[i++] = pbuf->buf[pbuf->rindex++];
		pbuf->rindex %= pbuf->capacity;
		atomic_dec(&pbuf->avail_data);
		atomic_inc(&pbuf->avail_space);
	} 

	waitq_wake_nr(&pbuf->poll_wait, 1);
	return len;
}

static ssize_t buf_write(struct gdbio_buffer *pbuf, const char *buffer, ssize_t size){
	ssize_t avail, len, written_bytes = 0;
	int i;
	do{
		wait_event_interruptible(pbuf->poll_wait, atomic_read(&pbuf->avail_space));

		i=0;
		avail = atomic_read(&pbuf->avail_space);
		len = (size > avail ? avail : size);
		while(i<len){
			pbuf->buf[pbuf->windex++] = buffer[i++];
			pbuf->windex %= pbuf->capacity;
			atomic_dec(&pbuf->avail_space); 
			atomic_inc(&pbuf->avail_data);
		}
		size -= len;
		written_bytes += len;
		waitq_wake_nr(&pbuf->poll_wait, 1);
	}while(size != 0);

	return written_bytes;    
}

/*
* gdbclient sending RSP packet to gdbserver 
*/
ssize_t gdbclient_write(struct file *filep, char *buffer, ssize_t size){
	if(gdbio_is_detached(filep)){
		return 0;    
	}

	struct gdb_fifo_file *file = filep->private_data; 
	struct gdbio_buffer* in_buf = file->in_buffer;   
	return buf_write(in_buf, buffer, size); 
}
/*
* gdbserver receiving RSP packet from gdbclient 
*/
ssize_t gdbserver_read(struct file *filep, char *buffer, ssize_t len){
	struct gdb_fifo_file *file = filep->private_data;
	struct gdbio_buffer* in_buf = file->in_buffer;   

	return buf_read(in_buf, buffer, len); 
}

/*
* gdbserver sending RSP packet to gdbclient 
*/
ssize_t gdbserver_write(struct file *filep, const char *buffer, ssize_t size){
	struct gdb_fifo_file *file = filep->private_data;
	struct gdbio_buffer* out_buf = file->out_buffer;   
	return buf_write(out_buf, buffer, size);
}

/*
* gdbclient receiving RSP packet from gdbserver 
*/
ssize_t gdbclient_read(struct file *filep, char *buffer, ssize_t len){
	if(gdbio_is_detached(filep)){
		return 0;    
	} 
	struct gdb_fifo_file *file = filep->private_data;
	struct gdbio_buffer* out_buf = file->out_buffer;   
	return buf_read(out_buf, buffer, len); 
}

static int open(struct inode * inodep, struct file * filep){
	struct gdb_fifo_inode_priv* priv = inodep->i_private;
	filep->private_data = priv->file_ptr;
    	
	return 0;
}

static struct kfs_fops gdb_fifo_fops = {
	.open = open,
	.write = NULL,
	.read = NULL,
	.poll = NULL,
	.close = NULL,
};


static int create(struct inode *inode, int mode ){
	struct gdb_fifo_inode_priv* priv = kmem_alloc( sizeof( *priv ) ); 
	if ( ! priv ) { 
		return -1;
	}

	dbg("\n");
	priv->file_ptr = kmem_alloc(sizeof(struct gdb_fifo_file));
	if ( ! priv->file_ptr  ) return -1;
    
	priv->file_ptr->in_buffer = alloc_gdbio_buffer( GDBIO_BUFFER_SIZE / 4);
	if(!priv->file_ptr->in_buffer){
		kmem_free(priv->file_ptr);
		kmem_free(priv);
		return -1;    
	}
	priv->file_ptr->out_buffer = alloc_gdbio_buffer( GDBIO_BUFFER_SIZE / 4);
	if(!priv->file_ptr->out_buffer){
		free_gdbio_buffer(priv->file_ptr->in_buffer); 
		kmem_free(priv->file_ptr);
		kmem_free(priv);
		return -1;    
	}
	inode->i_private = priv;
	return 0;
}

static int unlink(struct inode *inode ){
	struct gdb_fifo_inode_priv* priv = inode->i_private;
	free_gdbio_buffer( priv->file_ptr->in_buffer);
	free_gdbio_buffer( priv->file_ptr->out_buffer);

	kmem_free(priv->file_ptr);
	kmem_free(priv);
	return 0;
}


static struct inode_operations gdb_fifo_iops = {
	.create = create,
	.unlink = unlink,
};

static void get_gdb_fifo_path(const char *gdb_cons, char *path){
	sprintf(path, "/dev/%s",  gdb_cons);
}


struct file *open_gdb_fifo(const char *gdb_cons){
	char path[32];
	get_gdb_fifo_path(gdb_cons, path);
	struct file *fd = NULL;
	kfs_open_path(path, 0, O_RDWR, &fd);
	return fd;
}

int mk_gdb_fifo(const char *gdb_cons){
	char path[32];
	get_gdb_fifo_path(gdb_cons, path);

	struct file *console = NULL;
	kfs_open_path(path, 0, 0, &console);
	if(console){
		kfs_close(console);
		printk("GDB Error: %s is busy\n", path);
		return -1;    
	}

	if (!kfs_create(path, &gdb_fifo_iops, &gdb_fifo_fops, 0777, NULL, 0)){
		printk("GDB Error: creating %s failed\n", path); 
		return -ENOMEM;
	}
	return 0;
}

void rm_gdb_fifo(struct file *filep){ 
	struct inode *inode = filep->inode;
	inode->i_op->unlink(inode);
	htable_del(inode->parent->files, inode);
	atomic_set(&inode->i_count, 0);
	kfs_destroy(inode);
	kmem_free(filep); 
}

/*void gdbio_reset(struct file *filep){
	struct gdb_fifo_file *file = (struct gdb_fifo_file *) filep->private_data;   
	reset_gdbio_buffer(file->in_buffer);
	reset_gdbio_buffer(file->out_buffer);    
}*/

#ifdef CONFIG_PALACIOS_GDB
DRIVER_INIT("module", gdbio_module_init);
DRIVER_EXIT(gdbio_module_deinit);
#endif
