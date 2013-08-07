#!/usr/bin/perl -w

use strict;

my $BASEDIR    = `pwd`; chomp($BASEDIR);
my $INSTALLDIR = "$BASEDIR/../install";
my $INSTALLDIR_LINUX = "$BASEDIR/../install-linux";
my $XPMEM_DIR  = "/path/to/xpmem/install/directory";

my @packages;

my %portals4;
$portals4{name}          = "Portals4 SVN Trunk";
$portals4{directory}     = "portals4";
$portals4{get_cmd}       = "svn checkout http://portals4.googlecode.com/svn/trunk/ $portals4{directory}";
$portals4{preconfig_cmd} = "./autogen.sh";
$portals4{config_cmd}    = "./configure --enable-kitten --enable-ppe --disable-transport-ib --enable-transport-udp --disable-shared --with-xpmem=$INSTALLDIR --prefix=$INSTALLDIR";
push(@packages, \%portals4);

my %portals4_linux;
$portals4_linux{name}          = "Portals4 SVN Trunk";
$portals4_linux{directory}     = "portals4_linux";
$portals4_linux{get_cmd}       = "cp -r $portals4{directory} $portals4_linux{directory}; cd $portals4_linux{directory}; rm Makefile; cd ..";
$portals4_linux{preconfig_cmd} = "./autogen.sh";
$portals4_linux{config_cmd}    = "./configure --enable-ppe --disable-transport-ib --enable-transport-udp --disable-shared --with-xpmem=$XPMEM_DIR --prefix=$INSTALLDIR_LINUX";
push(@packages, \%portals4_linux);

my %ompi;
$ompi{name}              = "Open MPI SVN Trunk";
$ompi{directory}         = "ompi";
$ompi{get_cmd}           = "svn co http://svn.open-mpi.org/svn/ompi/trunk $ompi{directory}";
$ompi{preconfig_cmd}     = "./autogen.pl";
$ompi{config_cmd}        = "./configure --with-platform=snl/kitten --with-pmi=$INSTALLDIR --with-portals4=$INSTALLDIR
to portals>";
push(@packages, \%ompi);

my %shmem;
$shmem{name}               = "Portals SHMEM SVN Trunk";
$shmem{directory}          = "shmem";
$shmem{get_cmd}            = "svn checkout http://portals-shmem.googlecode.com/svn/trunk/ $shmem{directory}";
$shmem{preconfig_cmd}      = "autoreconf -if";
$shmem{config_cmd}         = "./configure LIBS=\"-L$INSTALLDIR/lib -lxpmem -lrt -lpthread\" --disable-shared --with-pmi=$INSTALLDIR --with-portals4=$INSTALLDIR --prefix=$INSTALLDIR";
push(@packages, \%shmem);

#my %gperftools;
#$gperftools{name}          = "Google Perf Tools SVN Trunk";
#$gperftools{directory}     = "gperftools";
#$gperftools{get_cmd}       = "svn checkout http://gperftools.googlecode.com/svn/trunk/ $gperftools{directory}";
#$gperftools{preconfig_cmd} = "./autogen.sh";
#$gperftools{config_cmd}    = "./configure --disable-cpu-profiler --disable-heap-profiler --disable-heap-checker --enable-static --disable-shared --prefix=$INSTALLDIR";
#push(@packages, \%gperftools);

#my %piapi;
#$piapi{name}              = "Kratos PIAPI SVN Trunk";
#$piapi{directory}         = "piapi";
#$piapi{get_cmd}           = "svn co svn+ssh://software.sandia.gov/svn/private/kratos $piapi{directory}";
#$piapi{preconfig_cmd}     = "autoconf";
#$piapi{config_cmd}        = "./configure --prefix=$INSTALLDIR";
#push(@packages, \%piapi);

# Download and build external packages
for (my $i=0; $i < @packages; $i++) {
	chdir "$BASEDIR";

	# Get the package
	my %pkg = %{$packages[$i]};
	if (! -d "$pkg{directory}") {
		print "[EX] Getting $pkg{name}.\n";
		system ($pkg{get_cmd});
	} else {
		print "[EX] Already got $pkg{name}.\n";
	}

	chdir "$pkg{directory}";

	# Pre-configure the package
	if (! -e "configure") {
		print "[EX] Running preconfig command for $pkg{name}.\n";
		system ($pkg{preconfig_cmd});
	} else {
		print "[EX] Already ran preconfig command for $pkg{name}.\n";
	}

	# Configure the package
	if (! -e "Makefile") {
		print "[EX] Running config command for $pkg{name}.\n";
		system ($pkg{config_cmd});
	} else {
		print "[EX] Already ran config command for $pkg{name}.\n";
	}
}
