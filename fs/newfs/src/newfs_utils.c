
#include "../include/newfs.h"

extern struct newfs_super      newfs_super; 
extern struct custom_options   newfs_options;

/**
 * @brief 获取文件名
 * 
 * @param path 
 * @return char* 
 */
char* newfs_get_fname(const char* path) {
    char ch = '/';
    char *q = strrchr(path, ch) + 1;
    return q;
}

/**
 * @brief 计算路径的层级
 * exm: /av/c/d/f
 * -> lvl = 4
 * @param path 
 * @return int 
 */
int newfs_calc_lvl(const char * path) {
    // char* path_cpy = (char *)malloc(strlen(path));
    // strcpy(path_cpy, path);
    char* str = path;
    int   lvl = 0;
    if (strcmp(path, "/") == 0) {
        return lvl;
    }
    while (*str != NULL) {
        if (*str == '/') {
            lvl++;
        }
        str++;
    }
    return lvl;
}

/**
 * @brief 驱动读
 * 
 * @param offset 
 * @param out_content 
 * @param size 
 * @return int 
 */
int newfs_driver_read(int offset, uint8_t *out_content, int size) {
    int      offset_aligned = NEWFS_ROUND_DOWN(offset, NEWFS_BLK_SZ());
    int      bias           = offset - offset_aligned;
    int      size_aligned   = NEWFS_ROUND_UP((size + bias), NEWFS_BLK_SZ());
    uint8_t* temp_content   = (uint8_t*)malloc(size_aligned);
    uint8_t* cur            = temp_content;
    // lseek(NEWFS_DRIVER(), offset_aligned, SEEK_SET);
    ddriver_seek(NEWFS_DRIVER(), offset_aligned, SEEK_SET);
    while (size_aligned != 0)
    {
        // read(NEWFS_DRIVER(), cur, NEWFS_BLK_SZ());
        ddriver_read(NEWFS_DRIVER(), cur, NEWFS_IO_SZ());                          // ddriver_read第三个参数size要等于设备IO单位的大小
        cur          += NEWFS_IO_SZ();
        size_aligned -= NEWFS_IO_SZ();   
    }
    memcpy(out_content, temp_content + bias, size);
    free(temp_content);
    return NEWFS_ERROR_NONE;
}
/**
 * @brief 驱动写
 * 
 * @param offset 
 * @param in_content 
 * @param size 
 * @return int 
 */
int newfs_driver_write(int offset, uint8_t *in_content, int size) {
    int      offset_aligned = NEWFS_ROUND_DOWN(offset, NEWFS_BLK_SZ());
    int      bias           = offset - offset_aligned;
    int      size_aligned   = NEWFS_ROUND_UP((size + bias), NEWFS_BLK_SZ());
    uint8_t* temp_content   = (uint8_t*)malloc(size_aligned);
    uint8_t* cur            = temp_content;
    newfs_driver_read(offset_aligned, temp_content, size_aligned);
    memcpy(temp_content + bias, in_content, size);
    
    // lseek(NEWFS_DRIVER(), offset_aligned, SEEK_SET);
    ddriver_seek(NEWFS_DRIVER(), offset_aligned, SEEK_SET);
    while (size_aligned != 0)
    {
        // write(NEWFS_DRIVER(), cur, NEWFS_IO_SZ());
        ddriver_write(NEWFS_DRIVER(), cur, NEWFS_IO_SZ());                           // ddriver_write第三个参数size要等于设备IO单位的大小
        cur          += NEWFS_IO_SZ();
        size_aligned -= NEWFS_IO_SZ();   
    }

    free(temp_content);
    return NEWFS_ERROR_NONE;
}

/**
 * @brief 为一个inode分配dentry，采用头插法
 * 
 * @param inode 
 * @param dentry 
 * @return int 
 */
int newfs_alloc_dentry(struct newfs_inode* inode, struct newfs_dentry* dentry) {
    if (inode->dentrys == NULL) {
        inode->dentrys = dentry;
    }
    else {
        dentry->brother = inode->dentrys;
        inode->dentrys = dentry;
    }
    inode->dir_cnt++;
    return inode->dir_cnt;
}
/**
 * @brief 将dentry从inode的dentrys中取出
 * 
 * @param inode 
 * @param dentry 
 * @return int 
 */
int newfs_drop_dentry(struct newfs_inode * inode, struct newfs_dentry * dentry) {
    boolean is_find = FALSE;
    struct newfs_dentry* dentry_cursor;
    dentry_cursor = inode->dentrys;
    
    if (dentry_cursor == dentry) {
        inode->dentrys = dentry->brother;
        is_find = TRUE;
    }
    else {
        while (dentry_cursor)
        {
            if (dentry_cursor->brother == dentry) {
                dentry_cursor->brother = dentry->brother;
                is_find = TRUE;
                break;
            }
            dentry_cursor = dentry_cursor->brother;
        }
    }
    if (!is_find) {
        return -NEWFS_ERROR_NOTFOUND;
    }
    inode->dir_cnt--;
    return inode->dir_cnt;
}
/**
 * @brief 分配一个inode，占用位图
 * 
 * @param dentry 该dentry指向分配的inode
 * @return newfs_inode
 */
struct newfs_inode* newfs_alloc_inode(struct newfs_dentry * dentry) {
    struct newfs_inode* inode;
    int byte_cursor = 0; 
    int bit_cursor  = 0; 
    int ino_cursor  = 0;
    int dno_cursor  = 0;
    int data_cnt    = 0;
    boolean is_find_free_entry = FALSE;
    boolean is_find_free_enough = FALSE;
    /*在inode位图上寻找未使用的inode节点*/
    for (byte_cursor = 0; byte_cursor < NEWFS_BLKS_SZ(newfs_super.map_inode_blks); 
         byte_cursor++)
    {
        for (bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
            if((newfs_super.map_inode[byte_cursor] & (0x1 << bit_cursor)) == 0) {    
                                                      /* 当前ino_cursor位置空闲 */
                newfs_super.map_inode[byte_cursor] |= (0x1 << bit_cursor);
                is_find_free_entry = TRUE;           
                break;
            }
            ino_cursor++;
        }
        if (is_find_free_entry) {
            break;
        }
    }

    /*为目录项分配inode节点*/
    if (!is_find_free_entry  || ino_cursor == newfs_super.max_ino)
        return -NEWFS_ERROR_NOSPACE;

    inode = (struct newfs_inode*)malloc(sizeof(struct newfs_inode));
    inode->ino  = ino_cursor; 
    inode->size = 0;

    /*为目录项分配inode节点并建立他们之间的连接*/
                                                      /* dentry指向inode */
    dentry->inode = inode;
    dentry->ino   = inode->ino;
                                                      /* inode指回dentry */
    inode->dentry = dentry;
    
    inode->dir_cnt = 0;
    inode->dentrys = NULL;
    
    // /*分配inode时不在data位图上分配节点，刷回磁盘的时候按需分配*/
    
    /*对于FILE类型要给其分配空间，不必分配连续的空间，分配一个指针即可，同时分配*/
    if (NEWFS_IS_REG(inode)) {
        for(int cnt=0;cnt<NEWFS_DATA_PER_FILE;cnt++){
            inode->data[cnt] = (uint8_t *)malloc(NEWFS_BLK_SZ());
        }
    }

    return inode;
}

/**
 * @brief 将内存inode及其下方结构全部刷回磁盘
 * 
 * @param inode 
 * @return int 
 */
int newfs_sync_inode(struct newfs_inode * inode) {
    struct newfs_inode_d  inode_d;
    struct newfs_dentry*  dentry_cursor;
    struct newfs_dentry_d dentry_d;
    int ino             = inode->ino;
    inode_d.ino         = ino;
    inode_d.size        = inode->size;
    inode_d.ftype       = inode->dentry->ftype;
    inode_d.dir_cnt     = inode->dir_cnt;
    int dno_cnt;
    for(dno_cnt=0; dno_cnt < NEWFS_DATA_PER_FILE; dno_cnt++){
        inode_d.dno[dno_cnt] = inode->dno[dno_cnt];
    }
    int offset;
    
    if (newfs_driver_write(NEWFS_INO_OFS(ino), (uint8_t *)&inode_d, 
                     sizeof(struct newfs_inode_d)) != NEWFS_ERROR_NONE) {
        NEWFS_DBG("[%s] io error\n", __func__);
        return -NEWFS_ERROR_IO;
    }

                                                      /* Cycle 1: 写 INODE */
                                                      /* Cycle 2: 写 数据 */
    if (NEWFS_IS_DIR(inode)) {       
        dno_cnt = 0;                   
        dentry_cursor = inode->dentrys;
        while (dentry_cursor != NULL && dno_cnt < NEWFS_DATA_PER_FILE){
            offset = NEWFS_DATA_OFS(inode->dno[dno_cnt]);
            /*逐层进行目录的写入*/
            while (dentry_cursor != NULL)
            {
                memcpy(dentry_d.fname, dentry_cursor->fname, NEWFS_MAX_FILE_NAME);
                dentry_d.ftype = dentry_cursor->ftype;
                dentry_d.ino = dentry_cursor->ino;
                if (newfs_driver_write(offset, (uint8_t *)&dentry_d, 
                                    sizeof(struct newfs_dentry_d)) != NEWFS_ERROR_NONE) {
                    NEWFS_DBG("[%s] io error\n", __func__);
                    return -NEWFS_ERROR_IO;                     
                }
            
                if (dentry_cursor->inode != NULL) {
                    newfs_sync_inode(dentry_cursor->inode);
                }

                dentry_cursor = dentry_cursor->brother;
                offset += sizeof(struct newfs_dentry_d);
                
                // 每次写入一个目录，为其在数据位图上分配一个节点
                int dno_cursor = 0;
                boolean is_find_free_enough = FALSE;
                for (int byte_cursor = 0; byte_cursor < NEWFS_BLKS_SZ(newfs_super.map_data_blks); 
                    byte_cursor++)
                {
                    for (int bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
                        if((newfs_super.map_data[byte_cursor] & (0x1 << bit_cursor)) == 0) {    
                                                                /* 当前dno_cursor位置空闲 */
                            newfs_super.map_data[byte_cursor] |= (0x1 << bit_cursor);
                            inode->dno[dno_cnt] = dno_cursor;  /*为当前目录项分配data节点*/
                            is_find_free_enough = TRUE;
                            break;
                        }
                        dno_cursor++;
                    }
                    if (is_find_free_enough) {
                        break;
                    }
                }
                if (!is_find_free_enough || dno_cursor == newfs_super.max_data)
                    return -NEWFS_ERROR_NOSPACE;

                if((offset+sizeof(struct newfs_dentry_d))>NEWFS_DATA_OFS(inode->dno[dno_cnt]+1)){       /*如果一个数据块不够写，写到下一个数据块*/
                    break;
                }
            }
            dno_cnt++;
        }
    }

    else if (NEWFS_IS_REG(inode)) {
        /*inode对应文件格式的写入*/
        for(dno_cnt=0;dno_cnt<NEWFS_DATA_PER_FILE;dno_cnt++){
            if (newfs_driver_write(NEWFS_DATA_OFS(inode->dno[dno_cnt]), inode->data[dno_cnt], 
                                NEWFS_BLK_SZ()) != NEWFS_ERROR_NONE) {
                NEWFS_DBG("[%s] io error\n", __func__);
                return -NEWFS_ERROR_IO;
            }

             // 每次写入一个数据块，为其在数据位图上分配一个节点
            int dno_cursor = 0;
            boolean is_find_free_enough = FALSE;
            for (int byte_cursor = 0; byte_cursor < NEWFS_BLKS_SZ(newfs_super.map_data_blks); 
                byte_cursor++)
            {
                for (int bit_cursor = 0; bit_cursor < UINT8_BITS; bit_cursor++) {
                    if((newfs_super.map_data[byte_cursor] & (0x1 << bit_cursor)) == 0) {    
                                                            /* 当前dno_cursor位置空闲 */
                        newfs_super.map_data[byte_cursor] |= (0x1 << bit_cursor);
                        inode->dno[dno_cnt] = dno_cursor;  /*为当前目录项分配data节点*/
                        is_find_free_enough = TRUE;
                        break;
                    }
                    dno_cursor++;
                }
                if (is_find_free_enough) {
                    break;
                }
            }
            if (!is_find_free_enough || dno_cursor == newfs_super.max_data)
                return -NEWFS_ERROR_NOSPACE;
        }
    }
    return NEWFS_ERROR_NONE;
}

/**
 * @brief 
 * 
 * @param dentry dentry指向ino，读取该inode
 * @param ino inode唯一编号
 * @return struct newfs_inode* 
 */
struct newfs_inode* newfs_read_inode(struct newfs_dentry * dentry, int ino) {
    struct newfs_inode* inode = (struct newfs_inode*)malloc(sizeof(struct newfs_inode));
    struct newfs_inode_d inode_d;
    struct newfs_dentry* sub_dentry;
    struct newfs_dentry_d dentry_d;
    int    dir_cnt = 0, dno_cnt = 0;

    /*通过磁盘驱动来将磁盘中ino号的inode读入内存*/
    if (newfs_driver_read(NEWFS_INO_OFS(ino), (uint8_t *)&inode_d, 
                        sizeof(struct newfs_inode_d)) != NEWFS_ERROR_NONE) {
        NEWFS_DBG("[%s] io error\n", __func__);
        return NULL;                    
    }
    inode->dir_cnt = 0;
    inode->ino = inode_d.ino;
    inode->size = inode_d.size;
    inode->dentry = dentry;
    inode->dentrys = NULL;
    for(dno_cnt=0; dno_cnt < NEWFS_DATA_PER_FILE;dno_cnt++){
        inode->dno[dno_cnt] = inode_d.dno[dno_cnt];
    }

    /*此处实现方式类似sync_icode，分两种文件类型分别讨论*/
    /*若是目录类型*/
    if (NEWFS_IS_DIR(inode)) {
        dir_cnt = inode_d.dir_cnt;
        dno_cnt = 0;
        int i = 0,off_cnt = 0;
        while(i < dir_cnt && dno_cnt < NEWFS_DATA_PER_FILE)      
        {
            if (newfs_driver_read(NEWFS_DATA_OFS(inode->dno[dno_cnt]) + off_cnt * sizeof(struct newfs_dentry_d), 
                                (uint8_t *)&dentry_d, 
                                sizeof(struct newfs_dentry_d)) != NEWFS_ERROR_NONE) {
                NEWFS_DBG("[%s] io error\n", __func__);
                return NULL;                    
            }
            sub_dentry = new_dentry(dentry_d.fname, dentry_d.ftype);
            sub_dentry->parent = inode->dentry;
            sub_dentry->ino    = dentry_d.ino; 
            newfs_alloc_dentry(inode, sub_dentry);
            i++;
            off_cnt++;
            /*如果一个数据块不够写，写到下一个数据块*/
            if(NEWFS_DATA_OFS(inode->dno[dno_cnt]) + (off_cnt+1) * sizeof(struct newfs_dentry_d) > NEWFS_DATA_OFS(inode->dno[dno_cnt]+1)){  
                dno_cnt++;
                off_cnt = 0;
            }
        }
    }
    /*若是文件类型直接读取数据即可*/
    else if (NEWFS_IS_REG(inode)) {
        /*对于文件的每一个数据块分别进行读取*/
        for(dno_cnt = 0;dno_cnt < NEWFS_DATA_PER_FILE;dno_cnt++){
            inode->data[dno_cnt] = (uint8_t *)malloc(NEWFS_BLK_SZ());
            if (newfs_driver_read(NEWFS_DATA_OFS(inode->dno[dno_cnt]), (uint8_t *)inode->data[dno_cnt], 
                                NEWFS_BLK_SZ()) != NEWFS_ERROR_NONE) {
                NEWFS_DBG("[%s] io error\n", __func__);
                return NULL;                    
            }
        }
    }
    return inode;
}

/**
 * @brief 获得inode节点对应的dentry
 * 
 * @param inode 
 * @param dir [0...]
 * @return struct newfs_dentry* 
 */
struct newfs_dentry* newfs_get_dentry(struct newfs_inode * inode, int dir) {
    struct newfs_dentry* dentry_cursor = inode->dentrys;
    int    cnt = 0;
    while (dentry_cursor)
    {
        if (dir == cnt) {
            return dentry_cursor;
        }
        cnt++;
        dentry_cursor = dentry_cursor->brother;
    }
    return NULL;
}

/**
 * @brief 
 * 路径解析函数，返回匹配的dentry
 * path: /qwe/ad  total_lvl = 2,
 *      1) find /'s inode       lvl = 1
 *      2) find qwe's dentry 
 *      3) find qwe's inode     lvl = 2
 *      4) find ad's dentry
 *
 * path: /qwe     total_lvl = 1,
 *      1) find /'s inode       lvl = 1
 *      2) find qwe's dentry
 * 
 * @param path 
 * @return struct newfs_inode* 
 */
struct newfs_dentry* newfs_lookup(const char * path, boolean* is_find, boolean* is_root) {
    struct newfs_dentry* dentry_cursor = newfs_super.root_dentry;
    struct newfs_dentry* dentry_ret = NULL;
    struct newfs_inode*  inode; 
    int   total_lvl = newfs_calc_lvl(path);
    int   lvl = 0;
    boolean is_hit;
    char* fname = NULL;
    char* path_cpy = (char*)malloc(sizeof(path));
    *is_root = FALSE;
    strcpy(path_cpy, path);                         /*分析路径函数*/

    if (total_lvl == 0) {                           /* 根目录 */
        *is_find = TRUE;
        *is_root = TRUE;
        dentry_ret = newfs_super.root_dentry;
    }
    fname = strtok(path_cpy, "/");       
    while (fname)
    {   
        lvl++;
        if (dentry_cursor->inode == NULL) {           /* Cache机制 */
            newfs_read_inode(dentry_cursor, dentry_cursor->ino);
        }

        inode = dentry_cursor->inode;
        /*若遍历到的inode节点是FILE类型，则结束遍历*/
        if (NEWFS_IS_REG(inode) && lvl < total_lvl) {
            NEWFS_DBG("[%s] not a dir\n", __func__);
            dentry_ret = inode->dentry;
            break;
        }
        /*若遍历到的inode节点是目录类型*/
        if (NEWFS_IS_DIR(inode)) {
            dentry_cursor = inode->dentrys;
            is_hit        = FALSE;

            while (dentry_cursor)
            {
                if (memcmp(dentry_cursor->fname, fname, strlen(fname)) == 0) {
                    is_hit = TRUE;
                    break;
                }
                dentry_cursor = dentry_cursor->brother;            /*遍历目录的子文件*/
            }
            /*未找到匹配路径*/
            if (!is_hit) {
                *is_find = FALSE;
                NEWFS_DBG("[%s] not found %s\n", __func__, fname);
                dentry_ret = inode->dentry;
                break;
            }
            /*找到完整匹配路径*/
            if (is_hit && lvl == total_lvl) {
                *is_find = TRUE;
                dentry_ret = dentry_cursor;
                break;
            }
        }
        fname = strtok(NULL, "/"); 
    }
    /*若函数运行时inode还未读进来，则需要重新读*/
    if (dentry_ret->inode == NULL) {
        dentry_ret->inode = newfs_read_inode(dentry_ret, dentry_ret->ino);
    }
    
    return dentry_ret;
}

/**
 * @brief 挂载newfs, Layout 如下
 * 
 * Layout
 * | Super | Inode Map | Data |
 * 
 * BLK_SZ = 2*Inode_SZ
 * 
 * 每个Inode占用一个Blk
 * @param options 
 * @return int 
 */
int newfs_mount(struct custom_options options){
    /*定义磁盘各部分结构*/
    int                 ret = NEWFS_ERROR_NONE;
    int                 driver_fd;
    struct newfs_super_d  newfs_super_d; 
    struct newfs_dentry*  root_dentry;
    struct newfs_inode*   root_inode;

    int                 inode_num;
    int                 data_num;
    int                 map_inode_blks;
    int                 map_data_blks;
    
    int                 super_blks;
    boolean             is_init = FALSE;

    newfs_super.is_mounted = FALSE;

    // driver_fd = open(options.device, O_RDWR);
    driver_fd = ddriver_open(options.device);   /*打开驱动*/

    if (driver_fd < 0) {
        return driver_fd;
    }

    /* 向内存超级块中标记驱动并写入磁盘大小和单次IO大小*/
    newfs_super.driver_fd = driver_fd;
    ddriver_ioctl(NEWFS_DRIVER(), IOC_REQ_DEVICE_SIZE,  &newfs_super.sz_disk);
    ddriver_ioctl(NEWFS_DRIVER(), IOC_REQ_DEVICE_IO_SZ, &newfs_super.sz_io);

    newfs_super.sz_blk = 2*newfs_super.sz_io;   //io的大小为512B，文件系统一个块位1024B
    /*创建根目录项并读取磁盘超级块到内存*/
    root_dentry = new_dentry("/", NEWFS_DIR);

    if (newfs_driver_read(NEWFS_SUPER_OFS, (uint8_t *)(&newfs_super_d), 
                        sizeof(struct newfs_super_d)) != NEWFS_ERROR_NONE) {
        return -NEWFS_ERROR_IO;
    }   

    /*根据超级块幻数判断是否为第一次启动磁盘，如果是第一次启动磁盘，则需要建立磁盘超级块的布局*/
                                                      /* 读取super */
    if (newfs_super_d.magic_num != NEWFS_MAGIC_NUM) {     /* 幻数无 */

        /* 为了简单起见，我们可以自行 规定位图 的大小 */
        super_blks = NEWFS_SUPER_BLKS;
        inode_num  =  NEWFS_INODE_BLKS;
        data_num = NEWFS_DATA_BLKS;
        map_inode_blks = NEWFS_MAP_INODE_BLKS;
        map_data_blks = NEWFS_MAP_DATA_BLKS;
        
                                                      /* 布局layout */
        newfs_super.max_ino = inode_num; 
        newfs_super.max_data = data_num; 
        
        newfs_super_d.map_inode_offset = NEWFS_SUPER_OFS + NEWFS_BLKS_SZ(super_blks);
        newfs_super_d.map_data_offset = newfs_super_d.map_inode_offset + NEWFS_BLKS_SZ(map_inode_blks);
        newfs_super_d.inode_offset = newfs_super_d.map_data_offset + NEWFS_BLKS_SZ(map_data_blks);
        newfs_super_d.data_offset = newfs_super_d.inode_offset + NEWFS_BLKS_SZ(inode_num);

        newfs_super_d.map_inode_blks  = map_inode_blks;
        newfs_super_d.map_data_blks  = map_data_blks;
        
        newfs_super_d.magic_num    = NEWFS_MAGIC_NUM;
        newfs_super_d.sz_usage    = 0;
        NEWFS_DBG("inode map blocks: %d\n", map_inode_blks);
        NEWFS_DBG("data map blocks: %d\n", map_data_blks);
        is_init = TRUE;
    }

    /*初始化内存中的超级块，和根目录项*/
    newfs_super.sz_usage   = newfs_super_d.sz_usage;      /* 建立 in-memory 结构 */
    
    newfs_super.map_inode = (uint8_t *)malloc(NEWFS_BLKS_SZ(newfs_super_d.map_inode_blks));
    newfs_super.map_data = (uint8_t *)malloc(NEWFS_BLKS_SZ(newfs_super_d.map_data_blks));
    newfs_super.map_inode_blks = newfs_super_d.map_inode_blks;
    newfs_super.map_data_blks = newfs_super_d.map_data_blks;
    newfs_super.map_inode_offset = newfs_super_d.map_inode_offset;
    newfs_super.map_data_offset = newfs_super_d.map_data_offset;
    newfs_super.inode_offset = newfs_super_d.inode_offset;
    newfs_super.data_offset = newfs_super_d.data_offset;

    if (newfs_driver_read(newfs_super_d.map_inode_offset, (uint8_t *)(newfs_super.map_inode), 
                        NEWFS_BLKS_SZ(newfs_super_d.map_inode_blks)) != NEWFS_ERROR_NONE) {
        return -NEWFS_ERROR_IO;
    }

    if (newfs_driver_read(newfs_super_d.map_data_offset, (uint8_t *)(newfs_super.map_data), 
                        NEWFS_BLKS_SZ(newfs_super_d.map_data_blks)) != NEWFS_ERROR_NONE) {
        return -NEWFS_ERROR_IO;
    }

    if (is_init) {                                    /* 分配根节点 */
        root_inode = newfs_alloc_inode(root_dentry);
        newfs_sync_inode(root_inode);                 /*将根节点写回磁盘*/
    }
    
    root_inode            = newfs_read_inode(root_dentry, NEWFS_ROOT_INO);
    root_dentry->inode    = root_inode;
    newfs_super.root_dentry = root_dentry;
    newfs_super.is_mounted  = TRUE;

    return ret;
}

/**
 * @brief 
 * 
 * @return int 
 */
int newfs_umount() {
    struct newfs_super_d  newfs_super_d; 

    if (!newfs_super.is_mounted) {
        return NEWFS_ERROR_NONE;
    }

    newfs_sync_inode(newfs_super.root_dentry->inode);     /* 从根节点向下刷写节点 */

    /*将内存超级块转换为磁盘超级块并写入磁盘*/                                                
    newfs_super_d.magic_num           = NEWFS_MAGIC_NUM;
    newfs_super_d.map_inode_blks      = newfs_super.map_inode_blks;
    newfs_super_d.map_data_blks       = newfs_super.map_data_blks;
    newfs_super_d.map_inode_offset    = newfs_super.map_inode_offset;
    newfs_super_d.map_data_offset     = newfs_super.map_data_offset;
    newfs_super_d.inode_offset        = newfs_super.inode_offset;
    newfs_super_d.data_offset         = newfs_super.data_offset;
    newfs_super_d.sz_usage            = newfs_super.sz_usage;

    if (newfs_driver_write(NEWFS_SUPER_OFS, (uint8_t *)&newfs_super_d, 
                     sizeof(struct newfs_super_d)) != NEWFS_ERROR_NONE) {
        return -NEWFS_ERROR_IO;
    }

    /*将inode位图和data位图写入磁盘*/
    if (newfs_driver_write(newfs_super_d.map_inode_offset, (uint8_t *)(newfs_super.map_inode), 
                         NEWFS_BLKS_SZ(newfs_super_d.map_inode_blks)) != NEWFS_ERROR_NONE) {
        return -NEWFS_ERROR_IO;
    }

    if (newfs_driver_write(newfs_super_d.map_data_offset, (uint8_t *)(newfs_super.map_data), 
                         NEWFS_BLKS_SZ(newfs_super_d.map_data_blks)) != NEWFS_ERROR_NONE) {
        return -NEWFS_ERROR_IO;
    }

    free(newfs_super.map_inode);
    free(newfs_super.map_data);

    /*关闭驱动*/
    ddriver_close(NEWFS_DRIVER());

    return NEWFS_ERROR_NONE;
}