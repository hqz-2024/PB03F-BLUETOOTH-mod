FILE: fs.h
if define FS_USE_RAID, it will indicate double fs;
if not define FS_USE_RAID, it will indicate single fs;

**** NEW API repleace API *****

 API                       | NEW API
hal_fs_get_free_size       | hal_fs_get_free_size_entry
hal_fs_garbage_collect     | hal_fs_garbage_collect_entry
hal_fs_item_write          | hal_fs_item_write_entry
hal_fs_init                | hal_fs_init_entry
hal_fs_item_read           | hal_fs_item_read_entry
hal_fs_item_del            | hal_fs_item_del_entry
hal_fs_get_garbage_size    | hal_fs_get_garbage_size_entry
hal_fs_format              | hal_fs_format_entry
 
