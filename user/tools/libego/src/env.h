#ifndef _ENV_H
#define _ENV_H ( 1 )

#define EGO_VERBOSE                     ( "EGO_VERBOSE" )
#define EGO_VERBOSE_DEFAULT             ( 0 )

#define EGO_TASK_LENGTH                 ( "EGO_TASK_LENGTH" )
#define EGO_TASK_LENGTH_DEFAULT         ( 100 )

#define SELFISH_TASK                    ( "selfish" )

#define EGO_SELFISH_THRESHOLD           ( "EGO_SELFISH_THRESHOLD" )
#define EGO_SELFISH_THRESHOLD_DEFAULT   ( 150 )

#define EGO_SELFISH_ASIZE               ( "EGO_SELFISH_ASIZE" )
#define EGO_SELFISH_ASIZE_DEFAULT       ( 128 * 1024 )

#define STENCIL_TASK                    ( "stencil" )

#define EGO_STENCIL_NDIMS               ( "EGO_STENCIL_NDIMS" )
#define EGO_STENCIL_NDIMS_DEFAULT       ( 3 )

#define EGO_STENCIL_NPX                 ( "EGO_STENCIL_NPX" )
#define EGO_STENCIL_NPX_DEFAULT         ( 1 )

#define EGO_STENCIL_NPY                 ( "EGO_STENCIL_NPY" )
#define EGO_STENCIL_NPY_DEFAULT         ( 1 )

#define EGO_STENCIL_NPZ                 ( "EGO_STENCIL_NPZ" )
#define EGO_STENCIL_NPZ_DEFAULT         ( 1 )

#define EGO_STENCIL_DURATION            ( "EGO_STENCIL_DURATION" )
#define EGO_STENCIL_DURATION_DEFAULT    ( 10000 )

#define FTQ_TASK                        ( "ftq" )

#define EGO_FTQ_DURATION                ( "EGO_FTQ_DURATION" )
#define EGO_FTQ_DURATION_DEFAULT        ( 10000 )

#define FWQ_TASK                        ( "fwq" )

#define EGO_FWQ_DURATION                ( "EGO_FWQ_DURATION" )
#define EGO_FWQ_DURATION_DEFAULT        ( 10000 )

#define EGO_TASK                        ( "EGO_TASK" )
#define EGO_TASK_DEFAULT                ( EGO_SELFISH_TASK )

#define EGO_PARTITION_ONE_OF_N          ( "1ofN" )

#define ONE_OF_N                        ( "EGO_ONE_OF_N" )
#define ONE_OF_N_DEFAULT                ( 16 )

#define EGO_PARTITION                   ( "EGO_PARTITION" )
#define EGO_PARTITION_DEFAULT           ( EGO_PARTITION_ONE_OF_N )

int set_params( params_union_t *, type_t *, dlist_t * );
#endif /* _ENV_H */
