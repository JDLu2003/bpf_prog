# xarray

## 基本设计

### xarray

```c
struct xarray {
    // 锁
    spinlock_t xa_lock;
    // gpf标志位
    gfp_t xa_flags;
    // 指向的 xa 头指针
    void __rcu * xa_head;  
}

### xa_node

```c
 struct xa_node {
    unsigned char shift;/* Bits remaining in each slot */
    unsigned char offset;/* Slot offset in parent */
    unsigned char count;/* Total entry count */
    unsigned char nr_values;/* Value entry count */
    struct xa_node __rcu *parent;/* NULL at top of tree */
    struct xarray *array;/* The array we belong to */
    union {
        struct list_head private_list;/* For tree user */
        struct rcu_head rcu_head;/* Used when freeing node */
    };
    void __rcu *slots[XA_CHUNK_SIZE];
    union {
        unsigned long tags[XA_MAX_MARKS][XA_MARK_LONGS];
        unsigned long marks[XA_MAX_MARKS][XA_MARK_LONGS];
    };
 };
```

## api

### 基础 api

插入

    void *xa_store(struct xarray *, unsigned long index, void *entry, gfp_t flags);

删除

    void *xa_erase(struct xarray *xa, unsigned long index);

修改

    void *xa_store(struct xarray *xa, unsigned long index, void *entry, gfp_t gfp);

查询

    void *xa_load(struct xarray *xa, unsigned long index);

### 高级 api

按范围查询

    int xa_find_range(struct xarray *xa, unsigned long start, unsigned long end, xa_tag_t tag, void *result[]);

迭代器

    int xa_for_each(struct xarray *xa, unsigned long *index, void *entry, xa_mark_t mark);

    int xa_for_each_marked(struct xarray *xa, unsigned long *index, void *entry, xa_mark_t mark, xa_mark_t *last);

## 在 pagecache 中的使用

首先会调用 `XA_STATE` 宏初始化 xa_state，设置其 xarray 字段和 index 字段

```c
static inline struct page *find_get_entry(struct xa_state *xas, pgoff_t max,
		xa_mark_t mark)
{
	struct page *page;

retry:
    /// 查找对应的页面
	if (mark == XA_PRESENT)
		page = xas_find(xas, max);
	else
		page = xas_find_marked(xas, max, mark);

	if (xas_retry(xas, page))
		goto retry;
	/*
	 * A shadow entry of a recently evicted page, a swap
	 * entry from shmem/tmpfs or a DAX entry.  Return it
	 * without attempting to raise page count.
	 */
    // 如果找到 page 并且这是一个值类型的 entry，就返回
	if (!page || xa_is_value(page))
		return page;

	/* Has the page moved or been split? */
	if (unlikely(page != xas_reload(xas))) {
		put_page(page);
		goto reset;
	}

	return page;
reset:
	xas_reset(xas);
	goto retry;
}
```