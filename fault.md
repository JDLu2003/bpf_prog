# fault 关键流程

```c
// arch/x86/mm/fault.c
// 注册异常处理函数 exc_page_fault
DEFINE_IDTENTRY_RAW_ERRORCODE(exc_page_fault)
{
    //...
    instrumentation_begin();
	handle_page_fault(regs, error_code, address);
	instrumentation_end();
    //...
}
```
<hr style="height: 4px; background: linear-gradient(90deg, #ff0000, #00ff00); border: none;">

进入 pagefault 处理函数

查看地址是内核空间还是用户空间，调用不同的处理函数

```c
static __always_inline void
handle_page_fault(struct pt_regs *regs, unsigned long error_code,
			      unsigned long address)
{
	// ...

	/* Was the fault on kernel-controlled part of the address space? */
	if (unlikely(fault_in_kernel_space(address))) {
		do_kern_addr_fault(regs, error_code, address);
	} else {
		do_user_addr_fault(regs, error_code, address);
		//...
	}
}
```

<hr style="height: 4px; background: linear-gradient(90deg, #ff0000, #00ff00); border: none;">

```c
static inline
void do_user_addr_fault(struct pt_regs *regs,
			unsigned long error_code,
			unsigned long address)
{
	// ... 一系列检查
retry:
	// ... 获得锁
good_area:
    // 确认合法
	if (unlikely(access_error(error_code, vma))) {
		bad_area_access_error(regs, error_code, address, vma);
		return;
	}

    // 进入处理程序
	fault = handle_mm_fault(vma, address, flags, regs);

	// ...
}
NOKPROBE_SYMBOL(do_user_addr_fault);
```

<hr style="height: 4px; background: linear-gradient(90deg, #ff0000, #00ff00); border: none;">

```c
/*
 * By the time we get here, we already hold the mm semaphore
 *
 * The mmap_lock may have been released depending on flags and our
 * return value.  See filemap_fault() and __lock_page_or_retry().
 */
vm_fault_t handle_mm_fault(struct vm_area_struct *vma, unsigned long address,
			   unsigned int flags, struct pt_regs *regs)
{
	vm_fault_t ret;

	// ...

	if (unlikely(is_vm_hugetlb_page(vma)))
		ret = hugetlb_fault(vma->vm_mm, vma, address, flags);
	else
		ret = __handle_mm_fault(vma, address, flags);

	// ... cgroup 处理

	// ... 统计信息

	return ret;
}
EXPORT_SYMBOL_GPL(handle_mm_fault);
```

<hr style="height: 4px; background: linear-gradient(90deg, #ff0000, #00ff00); border: none;">

构造 vmf 调用 handle_pte_fault

```c
static vm_fault_t __handle_mm_fault(struct vm_area_struct *vma,
		unsigned long address, unsigned int flags)
{
	// ... 构造 vmf
	return handle_pte_fault(&vmf);
}
```
<hr style="height: 4px; background: linear-gradient(90deg, #ff0000, #00ff00); border: none;">

```c
static vm_fault_t handle_pte_fault(struct vm_fault *vmf)
{
	pte_t entry;

	// ... 获取 pte

	if (!vmf->pte) {
		if (vma_is_anonymous(vmf->vma))
			return do_anonymous_page(vmf);
		else
			return do_fault(vmf);
	}
    // ... 其他处理情况
}
```
<hr style="height: 4px; background: linear-gradient(90deg, #ff0000, #00ff00); border: none;">

```c
static vm_fault_t do_fault(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
	struct mm_struct *vm_mm = vma->vm_mm;
	vm_fault_t ret;

	if (!vma->vm_ops->fault) {
		// 一些错误处理
	} else if (!(vmf->flags & FAULT_FLAG_WRITE))
		ret = do_read_fault(vmf);
	else if (!(vma->vm_flags & VM_SHARED))
		ret = do_cow_fault(vmf);
	else
		ret = do_shared_fault(vmf);

	// ... 后处理
	return ret;
}
```
<hr style="height: 4px; background: linear-gradient(90deg, #ff0000, #00ff00); border: none;">

在这里对错误处理进行分阶段完成

```c
static vm_fault_t do_read_fault(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
	vm_fault_t ret = 0;

	/*
	 * Let's call ->map_pages() first and use ->fault() as fallback
	 * if page by the offset is not ready to be mapped (cold cache or
	 * something).
	 */
	if (vma->vm_ops->map_pages && fault_around_bytes >> PAGE_SHIFT > 1) {
		if (likely(!userfaultfd_minor(vmf->vma))) {
			ret = do_fault_around(vmf);
			if (ret)
				return ret;
		}
	}
    // 执行缓存页面流程，保证页面加载
	ret = __do_fault(vmf);

	if (unlikely(ret & (VM_FAULT_ERROR | VM_FAULT_NOPAGE | VM_FAULT_RETRY)))
		return ret;

    // 执行映射流程， 将页面map 到虚拟地址空间
	ret |= finish_fault(vmf);

	unlock_page(vmf->page);
	if (unlikely(ret & (VM_FAULT_ERROR | VM_FAULT_NOPAGE | VM_FAULT_RETRY)))
		put_page(vmf->page);
	return ret;
}
```
<hr style="height: 4px; background: linear-gradient(90deg, #ff0000, #00ff00); border: none;">

```c
/*
 * The mmap_lock must have been held on entry, and may have been
 * released depending on flags and vma->vm_ops->fault() return value.
 * See filemap_fault() and __lock_page_retry().
 */
static vm_fault_t __do_fault(struct vm_fault *vmf)
{
	struct vm_area_struct *vma = vmf->vma;
	vm_fault_t ret;

	// ...
	ret = vma->vm_ops->fault(vmf);
	// ...
	return ret;
}
```

以上为进入

用户态、文件页、读、的 fault 流程。最终进入具体的 fault 操作

例如在 ext4 中

ext4_file_vm_ops 的fault方法设置为 filemap_fault 函数

在里面调用 do_sync_mmap_readahead 方法、filemap_read_page 方法

do_sync_mmap_readahead 会根据状态转呗预读窗口，最终调用 page_cache_ra_unbounded 读

<hr style="height: 4px; background: linear-gradient(90deg, #ff0000, #00ff00); border: none;">

进行 map 操作

```c
/**
 * finish_fault - finish page fault once we have prepared the page to fault
 *
 * @vmf: structure describing the fault
 *
 * This function handles all that is needed to finish a page fault once the
 * page to fault in is prepared. It handles locking of PTEs, inserts PTE for
 * given page, adds reverse page mapping, handles memcg charges and LRU
 * addition.
 *
 * The function expects the page to be locked and on success it consumes a
 * reference of a page being mapped (for the PTE which maps it).
 *
 * Return: %0 on success, %VM_FAULT_ code in case of error.
 */
vm_fault_t finish_fault(struct vm_fault *vmf)
```


# 细化 page_cache_ra_unbounded 函数

```c
/**
 * page_cache_ra_unbounded - Start unchecked readahead.
 * @ractl: Readahead control.
 * @nr_to_read: The number of pages to read.
 * @lookahead_size: Where to start the next readahead.
 *
 * This function is for filesystems to call when they want to start
 * readahead beyond a file's stated i_size.  This is almost certainly
 * not the function you want to call.  Use page_cache_async_readahead()
 * or page_cache_sync_readahead() instead.
 *
 * Context: File is referenced by caller.  Mutexes may be held by caller.
 * May sleep, but will not reenter filesystem to reclaim memory.
 */
void page_cache_ra_unbounded(struct readahead_control *ractl,
		unsigned long nr_to_read, unsigned long lookahead_size)
{
	struct address_space *mapping = ractl->mapping;
	unsigned long index = readahead_index(ractl);
	LIST_HEAD(page_pool);
	gfp_t gfp_mask = readahead_gfp_mask(mapping);
	unsigned long i;

	/*
	 * Partway through the readahead operation, we will have added
	 * locked pages to the page cache, but will not yet have submitted
	 * them for I/O.  Adding another page may need to allocate memory,
	 * which can trigger memory reclaim.  Telling the VM we're in
	 * the middle of a filesystem operation will cause it to not
	 * touch file-backed pages, preventing a deadlock.  Most (all?)
	 * filesystems already specify __GFP_NOFS in their mapping's
	 * gfp_mask, but let's be explicit here.
	 */
	unsigned int nofs = memalloc_nofs_save();

	filemap_invalidate_lock_shared(mapping);

	/*
	 * Preallocate as many pages as we will need.
	 */
	for (i = 0; i < nr_to_read; i++) {
		struct page *page = xa_load(&mapping->i_pages, index + i);

		if (page && !xa_is_value(page)) {
			 
			read_pages(ractl, &page_pool, true);
			i = ractl->_index + ractl->_nr_pages - index - 1;
			continue;
		}

		page = __page_cache_alloc(gfp_mask);
		if (!page)
			break;
		if (mapping->a_ops->readpages) {
			page->index = index + i;
			list_add(&page->lru, &page_pool);
		} else if (add_to_page_cache_lru(page, mapping, index + i, 
					gfp_mask) < 0) {
			put_page(page);
			read_pages(ractl, &page_pool, true);
			i = ractl->_index + ractl->_nr_pages - index - 1;
			continue;
		}
		if (i == nr_to_read - lookahead_size)
			SetPageReadahead(page);
		ractl->_nr_pages++;
	}

	/*
	 * Now start the IO.  We ignore I/O errors - if the page is not
	 * uptodate then the caller will launch readpage again, and
	 * will then handle the error.
	 */
	read_pages(ractl, &page_pool, false);
	filemap_invalidate_unlock_shared(mapping);
	memalloc_nofs_restore(nofs);
}
EXPORT_SYMBOL_GPL(page_cache_ra_unbounded);
```




