#ifndef _TYPES_H_
#define _TYPES_H_

#define MAX_NAME_LEN    128  
typedef int          boolean;
typedef uint16_t     flag16;

typedef enum newfs_file_type {
    NEWFS_REG_FILE,
    NEWFS_DIR,
} NEWFS_FILE_TYPE;

/******************************************************************************
* SECTION: Macro
*******************************************************************************/
#define TRUE                    1
#define FALSE                   0
#define UINT32_BITS             32
#define UINT8_BITS              8

#define NEWFS_MAGIC_NUM           0x2001113
#define NEWFS_SUPER_OFS           0
#define NEWFS_ROOT_INO            0

/* 规定位图各个部分的大小，自行指定而不估计 */
#define NEWFS_SUPER_BLKS          1
#define NEWFS_INODE_BLKS          512
#define NEWFS_DATA_BLKS           2048
#define NEWFS_MAP_INODE_BLKS      1
#define NEWFS_MAP_DATA_BLKS       2


#define NEWFS_ERROR_NONE          0
#define NEWFS_ERROR_ACCESS        EACCES
#define NEWFS_ERROR_SEEK          ESPIPE     
#define NEWFS_ERROR_ISDIR         EISDIR
#define NEWFS_ERROR_NOSPACE       ENOSPC
#define NEWFS_ERROR_EXISTS        EEXIST
#define NEWFS_ERROR_NOTFOUND      ENOENT
#define NEWFS_ERROR_UNSUPPORTED   ENXIO
#define NEWFS_ERROR_IO            EIO     /* Error Input/Output */
#define NEWFS_ERROR_INVAL         EINVAL  /* Invalid Args */

#define NEWFS_MAX_FILE_NAME       128
#define NEWFS_INODE_PER_FILE      1
#define NEWFS_DATA_PER_FILE       4
#define NEWFS_DEFAULT_PERM        0777

#define NEWFS_IOC_MAGIC           'S'
#define NEWFS_IOC_SEEK            _IO(NEWFS_IOC_MAGIC, 0)

#define NEWFS_FLAG_BUF_DIRTY      0x1
#define NEWFS_FLAG_BUF_OCCUPY     0x2   

/******************************************************************************
* SECTION: Macro Function
*******************************************************************************/
#define NEWFS_IO_SZ()                     (newfs_super.sz_io)             /*inode的大小512B*/
#define NEWFS_BLK_SZ()                    (newfs_super.sz_blk)            /*EXT2文件系统一个块大小1024B*/
#define NEWFS_DISK_SZ()                   (newfs_super.sz_disk)           /*磁盘大小4MB*/
#define NEWFS_DRIVER()                    (newfs_super.driver_fd)

#define NEWFS_ROUND_DOWN(value, round)    (value % round == 0 ? value : (value / round) * round)
#define NEWFS_ROUND_UP(value, round)      (value % round == 0 ? value : (value / round + 1) * round)

#define NEWFS_BLKS_SZ(blks)               (blks * NEWFS_BLK_SZ())
#define NEWFS_ASSIGN_FNAME(pnewfs_dentry, _fname)   memcpy(pnewfs_dentry->fname, _fname, strlen(_fname))
#define NEWFS_INO_OFS(ino)                (newfs_super.inode_offset + ino * NEWFS_BLK_SZ())    /*求ino对应inode偏移位置*/
#define NEWFS_DATA_OFS(dno)               (newfs_super.data_offset + dno * NEWFS_BLK_SZ())     /*求dno对应data偏移位置*/

#define NEWFS_IS_DIR(pinode)              (pinode->dentry->ftype == NEWFS_DIR)
#define NEWFS_IS_REG(pinode)              (pinode->dentry->ftype == NEWFS_REG_FILE)
/******************************************************************************
* SECTION: FS Specific Structure - In memory structure 内存
*******************************************************************************/
struct newfs_dentry;
struct newfs_inode;
struct newfs_super;

struct custom_options {
	const char*        device;
	boolean            show_help;
};

struct newfs_inode {
    /* TODO: Define yourself */
    uint32_t                ino;                           /* 在inode位图中的下标 */
    int                     size;                          /* 文件已占用空间 */
    int                     dir_cnt;                       /* 如果是目录类型文件，下面有几个目录项 */
    NEWFS_FILE_TYPE         ftype;                         /* 文件类型 */
    struct newfs_dentry* dentry;                      /* 指向该inode的dentry */
    struct newfs_dentry* dentrys;                     /* 所有目录项 */
    uint8_t*                data[NEWFS_DATA_PER_FILE];     /* 如果是FILE文件，数据块指针 */
    int                     dno[NEWFS_DATA_PER_FILE];      /* inode指向文件的各个数据块在数据位图中的下标 */    
};

struct newfs_dentry {
    /* TODO: Define yourself */
    char                    fname[NEWFS_MAX_FILE_NAME];
    NEWFS_FILE_TYPE         ftype;                          /* 文件类型 */
    struct newfs_dentry* parent;                            /* 父亲Inode的dentry */
    struct newfs_dentry* brother;                           /* 兄弟 */
    uint32_t                ino;                            /* 指向的ino号 */
    struct newfs_inode*     inode;                          /* 指向inode */
    int                     valid;                          /* 该目录项是否有效 */  
};

struct newfs_super {
    /* TODO: Define yourself */
    int                driver_fd;

    int                sz_io;                   /*inode的大小*/
    int                sz_disk;                 /*磁盘大小*/
    int                sz_blk;                  /*EXT2文件系统一个块大小*/
    int                sz_usage;

    int                max_ino;                 /*inode的数目，即最多支持的文件数*/
    uint8_t*           map_inode;               /*inode位图*/
    int                map_inode_blks;          /*inode位图所占的数据块*/
    int                map_inode_offset;        /*inode位图的偏移,即起始地址*/

    int                max_data;               /*data索引的数目*/
    uint8_t*           map_data;               /*data位图*/
    int                map_data_blks;          /*数据位图所占的数据块*/
    int                map_data_offset;        /*数据位图的偏移,即起始地址*/

    int                inode_offset;            /*inode块区的偏移,即起始地址*/
    int                data_offset;             /*数据块的偏移,即起始地址*/

    boolean            is_mounted;

    struct newfs_dentry* root_dentry;             /*根目录*/
};

static inline struct newfs_dentry* new_dentry(char * fname, NEWFS_FILE_TYPE ftype) {
    struct newfs_dentry * dentry = (struct newfs_dentry *)malloc(sizeof(struct newfs_dentry));
    memset(dentry, 0, sizeof(struct newfs_dentry));
    NEWFS_ASSIGN_FNAME(dentry, fname);
    dentry->ftype   = ftype;
    dentry->ino     = -1;
    dentry->inode   = NULL;
    dentry->parent  = NULL;
    dentry->brother = NULL;     
    return  dentry;                                       
}

/******************************************************************************
* SECTION: FS Specific Structure - Disk structure 磁盘
*******************************************************************************/
struct newfs_super_d {
    /* TODO: Define yourself */
    uint32_t           magic_num;
    int                sz_usage;

    uint32_t           max_ino;                 /*inode的数目，即最多支持的文件数*/
    int                map_inode_blks;          /*inode位图所占的数据块*/
    int                map_inode_offset;        /*inode位图的偏移*/

    int                map_data_blks;          /*数据位图所占的数据块*/
    int                map_data_offset;        /*数据位图的偏移*/

    int                inode_offset;            /*inode块的偏移*/
    int                data_offset;             /*数据块的偏移*/
};

struct newfs_inode_d {
    /* TODO: Define yourself */
    uint32_t                ino;                           /* 在inode位图中的下标 */
    int                     size;                          /* 文件已占用空间 */
    int                     dir_cnt;                       /* 如果是目录类型文件，下面有几个目录项 */
    NEWFS_FILE_TYPE         ftype;                         /* 文件类型 */
    int                     dno[NEWFS_DATA_PER_FILE];      /* inode指向文件的各个数据块在数据位图中的下标 */    
};

struct newfs_dentry_d {
    /* TODO: Define yourself */
    char                fname[NEWFS_MAX_FILE_NAME];
    NEWFS_FILE_TYPE     ftype;                                      /* 文件类型 */
    uint32_t            ino;                                        /* 指向的ino号 */
    int                 valid;                                      /* 该目录项是否有效 */  
};
#endif /* _TYPES_H_ */