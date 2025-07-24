# 解释 reval 在 filemap_read 中的变化
_Exported on 2025/7/15 at GMT+8 11:38:36 from Cursor (1.2.4)_

---

**User**

请你解释这个 reval 最终在传入
	return filemap_read(iocb, iter, retval);的时候会变成什么

---

**Cursor**

好的，我来详细解释 `retval` 在 `return filemap_read(iocb, iter, retval);` 这一行时的含义和传递过程。

## 1. 代码位置与上下文

你给出的代码是 Linux 内核 `mm/filemap.c` 文件中 `generic_file_read_iter` 函数的实现：

```c
ssize_t
generic_file_read_iter(struct kiocb *iocb, struct iov_iter *iter)
{
	size_t count = iov_iter_count(iter);
	ssize_t retval = 0;

	if (!count)
		return 0; /* skip atime */

	if (iocb->ki_flags & IOCB_DIRECT) {
		// ... 省略 ...
		retval = kiocb_write_and_wait(iocb, count);
		if (retval < 0)
			return retval;
		file_accessed(file);

		retval = mapping->a_ops->direct_IO(iocb, iter);
		if (retval >= 0) {
			iocb->ki_pos += retval;
			count -= retval;
		}
		if (retval != -EIOCBQUEUED)
			iov_iter_revert(iter, count - iov_iter_count(iter));

		// ... 省略 ...
		if (retval < 0 || !count || IS_DAX(inode))
			return retval;
		if (iocb->ki_pos >= i_size_read(inode))
			return retval;
	}

	return filemap_read(iocb, iter, retval);
}
```

## 2. retval 的来源

- `retval` 初始为 0。
- 如果是直接IO（`IOCB_DIRECT`），会先调用 `kiocb_write_and_wait`，如果出错直接返回。
- 然后调用 `mapping->a_ops->direct_IO(iocb, iter)`，把返回值赋给 `retval`。
- 如果 `retval >= 0`，说明直接IO读了一些数据，`iocb->ki_pos` 会增加，`count` 也会减少。
- 如果 `retval < 0`，说明出错，直接返回。
- 如果 `retval >= 0`，但 `count == 0`（即已经读完），也直接返回。
- 如果 `retval >= 0`，但没读完，且不是 DAX 文件，也没到 EOF，就会进入最后一行：

  ```c
  return filemap_read(iocb, iter, retval);
  ```

## 3. retval 传入 filemap_read 的意义

- 这里的 `retval` 代表**已经通过 direct IO 读到的数据字节数**。
- 由于有些文件系统（如 btrfs）可能 direct IO 只读了一部分数据，剩下的需要用 buffered IO 继续读。
- 所以 `filemap_read` 需要知道**前面已经读了多少字节**，这样它在出错时能正确返回“总共读了多少字节”，而不是只返回 buffered IO 的部分。

## 4. filemap_read 里怎么用 retval

在 `filemap_read` 里，`already_read` 参数就是 `retval`：

```c
ssize_t filemap_read(struct kiocb *iocb, struct iov_iter *iter, ssize_t already_read)
{
	// ... 省略 ...
	do {
		// ... 省略 ...
		error = filemap_get_pages(iocb, iter->count, &fbatch, false);
		if (error < 0)
			break;
		// ... 省略 ...
		for (i = 0; i < folio_batch_count(&fbatch); i++) {
			// ... 省略 ...
			copied = copy_folio_to_iter(folio, offset, bytes, iter);

			already_read += copied;
			iocb->ki_pos += copied;
			last_pos = iocb->ki_pos;

			if (copied < bytes) {
				error = -EFAULT;
				break;
			}
		}
		// ... 省略 ...
	} while (iov_iter_count(iter) && iocb->ki_pos < isize && !error);

	// ... 省略 ...
	return already_read ? already_read : error;
}
```

- `already_read`（也就是 `retval`）会累加 buffered IO 读到的字节数。
- 最终返回值是**direct IO + buffered IO 总共读到的字节数**。

## 5. 总结

- `retval` 在传入 `filemap_read` 时，**表示前面 direct IO 已经读到的字节数**。
- `filemap_read` 会在此基础上继续 buffered IO 读取，并把总共读到的字节数返回。
- 这样可以保证即使 direct IO 只读了一部分，用户也能拿到“实际读到的总字节数”。

---

**一句话总结：**  
`retval` 在 `return filemap_read(iocb, iter, retval);` 这行时，表示“前面 direct IO 已经读到的字节数”，`filemap_read` 会在此基础上继续读，最终返回“总共读到的字节数”。

```c
ssize_t
generic_file_read_iter(struct kiocb *iocb, struct iov_iter *iter)
{
	size_t count = iov_iter_count(iter);
	ssize_t retval = 0;

	if (!count)
		return 0; /* skip atime */

	if (iocb->ki_flags & IOCB_DIRECT) {
		// ... 省略 ...
		retval = kiocb_write_and_wait(iocb, count);
		if (retval < 0)
			return retval;
		file_accessed(file);

		retval = mapping->a_ops->direct_IO(iocb, iter);
		if (retval >= 0) {
			iocb->ki_pos += retval;
			count -= retval;
		}
		if (retval != -EIOCBQUEUED)
			iov_iter_revert(iter, count - iov_iter_count(iter));

		// ... 省略 ...
		if (retval < 0 || !count || IS_DAX(inode))
			return retval;
		if (iocb->ki_pos >= i_size_read(inode))
			return retval;
	}

	return filemap_read(iocb, iter, retval);
}
```

```c
  return filemap_read(iocb, iter, retval);
```

```c
ssize_t filemap_read(struct kiocb *iocb, struct iov_iter *iter, ssize_t already_read)
{
	// ... 省略 ...
	do {
		// ... 省略 ...
		error = filemap_get_pages(iocb, iter->count, &fbatch, false);
		if (error < 0)
			break;
		// ... 省略 ...
		for (i = 0; i < folio_batch_count(&fbatch); i++) {
			// ... 省略 ...
			copied = copy_folio_to_iter(folio, offset, bytes, iter);

			already_read += copied;
			iocb->ki_pos += copied;
			last_pos = iocb->ki_pos;

			if (copied < bytes) {
				error = -EFAULT;
				break;
			}
		}
		// ... 省略 ...
	} while (iov_iter_count(iter) && iocb->ki_pos < isize && !error);

	// ... 省略 ...
	return already_read ? already_read : error;
}
```

---

**User**

使用中文描述这个过程

---

**Cursor**

好的，下面用中文详细描述 `retval`（也就是 `already_read`）在 `filemap_read` 里的整个流转和作用：

---

## 1. 入口参数

- `filemap_read(struct kiocb *iocb, struct iov_iter *iter, ssize_t already_read)`
- 其中 `already_read` 就是前面已经通过 direct IO 读到的字节数（如果没有 direct IO，就是 0）。

---

## 2. 主要流程

1. **初始化和边界检查**
   - 如果当前位置已经超过文件系统最大字节数，直接返回 0。
   - 如果 `iter` 没有要读的数据，也直接返回 0。

2. **循环读取数据**
   - 进入 do-while 循环，每次循环尝试从 page cache 里批量读取一组 folio（页）。
   - 如果已经读到部分数据（`already_read > 0`），并且是异步读（`IOCB_WAITQ`），则强制设置 `IOCB_NOWAIT`，表示后续不能再阻塞。

3. **获取数据页**
   - 调用 `filemap_get_pages` 获取一批可读的 folio。
   - 如果出错（返回负数），跳出循环。

4. **处理每个 folio**
   - 遍历本次批量获取到的每个 folio，计算本次要从该 folio 读取的字节数 `bytes`。
   - 调用 `copy_folio_to_iter` 把数据从 folio 拷贝到用户空间。
   - **关键：**每次成功拷贝后，`already_read += copied;`，即把本次拷贝的字节数累加到 `already_read`。
   - 同时更新 `iocb->ki_pos`（文件偏移）和 `last_pos`。

5. **出错处理**
   - 如果本次拷贝的字节数小于应拷贝的字节数，说明出错（如用户空间不可写），设置 `error = -EFAULT`，跳出循环。

6. **释放引用**
   - 每次循环结束后，释放本次批量获取的 folio 的引用。

7. **循环条件**
   - 只要 `iter` 还有剩余空间、文件没到结尾、没有出错，就继续循环。

---

## 3. 返回值

- 循环结束后，调用 `file_accessed` 更新文件访问时间。
- 最终返回 `already_read ? already_read : error;`
  - 如果已经读到数据（`already_read > 0`），就返回总共读到的字节数（包括 direct IO 和 buffered IO）。
  - 如果一次都没读到数据，返回最后一次的错误码。

---

## 4. 总结

**整个过程的核心：**
- `already_read` 作为累计变量，记录“到目前为止已经成功读到的总字节数”。
- 每次从 folio 拷贝数据后都累加到 `already_read`。
- 最终返回值就是“本次 read 操作实际读到的总字节数”，即使中间 direct IO 只读了一部分，buffered IO 又补了一部分，也能正确反映总量。

---

### 形象比喻

你可以把 `already_read` 理解为一个“计步器”，每读到一点数据就加上，最后告诉你“这次总共走了多少步”。  
如果中途遇到障碍（出错），也会告诉你“虽然没走完，但已经走了多少步”。

---

如有更细节的地方需要解释，欢迎继续提问！

---

**User**


static int filemap_get_pages(struct kiocb *iocb, size_t count,
		struct folio_batch *fbatch, bool need_uptodate)
{
	struct file *filp = iocb->ki_filp;
	struct address_space *mapping = filp->f_mapping;
	struct file_ra_state *ra = &filp->f_ra;
	pgoff_t index = iocb->ki_pos >> PAGE_SHIFT;
	pgoff_t last_index;
	struct folio *folio;
	int err = 0;

	/* "last_index" is the index of the page beyond the end of the read */
	last_index = DIV_ROUND_UP(iocb->ki_pos + count, PAGE_SIZE);
retry:
	if (fatal_signal_pending(current))
		return -EINTR;

	filemap_get_read_batch(mapping, index, last_index - 1, fbatch);
	if (!folio_batch_count(fbatch)) {
		if (iocb->ki_flags & IOCB_NOIO)
			return -EAGAIN;
		page_cache_sync_readahead(mapping, ra, filp, index,
				last_index - index);
		filemap_get_read_batch(mapping, index, last_index - 1, fbatch);
	}
	if (!folio_batch_count(fbatch)) {
		if (iocb->ki_flags & (IOCB_NOWAIT | IOCB_WAITQ))
			return -EAGAIN;
		err = filemap_create_folio(filp, mapping,
				iocb->ki_pos >> PAGE_SHIFT, fbatch);
		if (err == AOP_TRUNCATED_PAGE)
			goto retry;
		return err;
	}

	folio = fbatch->folios[folio_batch_count(fbatch) - 1];
	if (folio_test_readahead(folio)) {
		err = filemap_readahead(iocb, filp, mapping, folio, last_index);
		if (err)
			goto err;
	}
	if (!folio_test_uptodate(folio)) {
		if ((iocb->ki_flags & IOCB_WAITQ) &&
		    folio_batch_count(fbatch) > 1)
			iocb->ki_flags |= IOCB_NOWAIT;
		err = filemap_update_page(iocb, mapping, count, folio,
					  need_uptodate);
		if (err)
			goto err;
	}

	return 0;
err:
	if (err < 0)
		folio_put(folio);
	if (likely(--fbatch->nr))
		return 0;
	if (err == AOP_TRUNCATED_PAGE)
		goto retry;
	return err;
}请你用中文解释这个流程，并且你需要解释调用的函数

---

**Cursor**

好的，下面我用中文详细解释 `filemap_get_pages` 这个函数的整体流程，并对关键调用的函数做简要说明。

---

## 1. 作用概述

`filemap_get_pages` 的作用是：  
**从文件的 page cache（页缓存）中获取一批可用于读操作的 folio（页/块），如果没有则尝试触发预读或新建页。**  
它是 Linux 内核文件读流程中的一个核心环节。

---

## 2. 参数说明

- `struct kiocb *iocb`：本次 IO 操作的上下文，包含文件、偏移、标志等。
- `size_t count`：本次希望读取的总字节数。
- `struct folio_batch *fbatch`：用于存放本次批量获取到的 folio。
- `bool need_uptodate`：是否要求页内容是最新的（通常为 false，只有特殊场景才为 true）。

---

## 3. 主要流程

### 1）计算本次要读取的页范围

- `index`：当前读指针所在的页号（页索引）。
- `last_index`：本次读操作结束后，**下一个**页的索引（即本次读的页范围是 `[index, last_index-1]`）。

### 2）信号中断检查

- 如果当前进程有致命信号挂起，直接返回 `-EINTR`，表示被信号打断。

### 3）尝试批量获取页

- 调用 `filemap_get_read_batch(mapping, index, last_index - 1, fbatch);`
  - **作用**：从 page cache 里批量获取 `[index, last_index-1]` 范围内的 folio，放入 `fbatch`。
  - **说明**：只会获取已经在缓存里的页，不会触发 IO。

### 4）如果没有获取到任何页

- 说明缓存里没有可用页，需要考虑预读或新建页。

#### a. 如果设置了 `IOCB_NOIO`（不允许新 IO）

- 直接返回 `-EAGAIN`，表示不能阻塞等待 IO。

#### b. 否则，触发同步预读

- 调用 `page_cache_sync_readahead(mapping, ra, filp, index, last_index - index);`
  - **作用**：根据当前读位置和历史访问模式，触发一次同步预读，把后续可能要用到的页提前读入缓存。
- 预读后再次尝试获取页（再调一次 `filemap_get_read_batch`）。

### 5）如果还是没有获取到页

- 说明预读也没能拿到页，可能是文件有空洞或页还没分配。

#### a. 如果设置了 `IOCB_NOWAIT` 或 `IOCB_WAITQ`（不允许阻塞）

- 直接返回 `-EAGAIN`。

#### b. 否则，尝试新建一个 folio 并读入数据

- 调用 `filemap_create_folio(filp, mapping, iocb->ki_pos >> PAGE_SHIFT, fbatch);`
  - **作用**：分配一个新的 folio，插入 page cache，并从底层存储读入数据。
  - **返回值**：如果返回 `AOP_TRUNCATED_PAGE`，说明页被截断，重试；否则返回错误码或 0。

### 6）如果获取到了页

- 取出本批次最后一个 folio（通常是本次要读的最后一页）。

#### a. 如果该 folio 处于 readahead 状态（预读页）

- 调用 `filemap_readahead(iocb, filp, mapping, folio, last_index);`
  - **作用**：根据当前访问模式，可能会触发更多的异步预读。
  - **如果出错**，跳转到 `err` 处理。

#### b. 如果该 folio 还不是 uptodate（数据还没准备好）

- 如果是异步读且本批次有多页，设置 `IOCB_NOWAIT`，后续不再阻塞。
- 调用 `filemap_update_page(iocb, mapping, count, folio, need_uptodate);`
  - **作用**：确保该 folio 的数据是最新的（必要时会触发底层 IO 读入数据）。
  - **如果出错**，跳转到 `err` 处理。

### 7）正常返回

- 如果一切顺利，返回 0，表示本批次页已准备好。

---

## 4. 错误处理

- 如果在处理过程中出错（`err` 标签）：
  - 如果 `err < 0`，说明有错误，需要释放本次获取的 folio 的引用（`folio_put(folio)`）。
  - 如果本批次还有剩余 folio（`--fbatch->nr`），说明还有可用页，返回 0。
  - 如果错误是 `AOP_TRUNCATED_PAGE`，说明页被截断，重试。
  - 其他情况，返回错误码。

---

## 5. 关键调用函数简要说明

- **filemap_get_read_batch**  
  从 page cache 里批量获取一组 folio，不会触发 IO，只拿已经在缓存里的页。

- **page_cache_sync_readahead**  
  根据访问模式和历史，触发一次同步预读，把后续可能要用到的页提前读入缓存。

- **filemap_create_folio**  
  分配一个新的 folio，插入 page cache，并从底层存储读入数据，保证该页可用。

- **filemap_readahead**  
  针对 readahead 状态的 folio，可能会触发更多的异步预读，优化顺序读性能。

- **filemap_update_page**  
  确保 folio 的数据是最新的，如果还没准备好会触发底层 IO 读入数据。

- **folio_put**  
  释放 folio 的引用计数，防止内存泄漏。

---

## 6. 总结

**filemap_get_pages** 的核心流程就是：
1. 先尝试直接从缓存拿页；
2. 拿不到就预读或新建页；
3. 保证拿到的页是可用的、最新的；
4. 如果遇到不能阻塞的场景，及时返回 `-EAGAIN`；
5. 如果遇到信号或其他错误，及时返回错误码。

这个函数是 Linux 文件读流程中“从缓存/底层拿到可用数据页”的关键一环。

如需对某个子函数进一步详细解释，可以继续提问！

---

**User**

请问 filemap_read和 filemap_fault的流程有什么不同

---

**Cursor**

这是一个非常经典的内核问题，下面用中文详细对比说明 `filemap_read` 和 `filemap_fault` 这两个函数的主要流程和区别。

---

## 一、filemap_read 的流程

`filemap_read` 主要用于**普通的文件读操作**（如 read、pread、readv 等系统调用），其流程如下：

1. **参数准备**  
   由 VFS 层传入 `kiocb`（IO上下文）、`iov_iter`（用户缓冲区）、`already_read`（已读字节数）。

2. **循环批量读取页**  
   - 通过 `filemap_get_pages` 批量获取当前要读的数据页（folio）。
   - 如果页不在缓存，可能会触发预读（readahead）或新建页并从磁盘读取。
   - 如果页还没准备好（不是uptodate），会等待IO完成。

3. **数据拷贝**  
   - 将页中的数据拷贝到用户空间。
   - 更新已读字节数、文件偏移。

4. **循环直到全部数据读完或出错**  
   - 继续下一批页，直到用户缓冲区满、文件结尾或遇到错误。

5. **返回已读字节数或错误码**

**核心特点：**
- 以“批量页”为单位，尽量高效地从页缓存中读取数据。
- 允许预读和异步IO优化。
- 主要服务于系统调用层的 read 语义。

---

## 二、filemap_fault 的流程

`filemap_fault` 主要用于**内存映射（mmap）时的缺页异常处理**，即当进程访问 mmap 映射的文件区域、但对应页还没在内存时，由内核 page fault 处理流程调用。

1. **参数准备**  
   由内核缺页异常处理流程传入 `vm_fault` 结构体，包含虚拟地址、VMA、页号等信息。

2. **查找页缓存**  
   - 先尝试在页缓存中查找目标页（folio）。
   - 如果找到了，尝试加锁，检查是否 uptodate。

3. **预读优化**  
   - 如果页不在缓存，统计 major fault，可能触发同步预读（readahead）。
   - 需要加 invalidate_lock，防止与截断等操作冲突。

4. **新建页并读取数据**  
   - 如果页还不存在，分配新页并从底层读取数据。

5. **等待页准备好**  
   - 如果页还没准备好（不是uptodate），会同步等待IO完成。

6. **将页映射到进程虚拟地址空间**  
   - 检查页是否被截断、是否越界。
   - 将页插入到进程的页表中，完成缺页处理。

7. **返回 VM_FAULT_XXX 标志**  
   - 告诉内核缺页处理的结果（如是否需要重试、是否出错等）。

**核心特点：**
- 以“单页”为单位，服务于缺页异常（page fault）处理。
- 需要考虑与 mmap 相关的锁、同步、页表操作。
- 主要服务于进程通过 mmap 映射文件后的按需加载。

---

## 三、主要区别总结

| 对比点         | filemap_read（普通读）         | filemap_fault（缺页异常）         |
|----------------|-------------------------------|-----------------------------------|
| 触发场景       | read/pread/readv 等系统调用    | 进程访问 mmap 映射文件时缺页      |
| 处理单位       | 批量页（folio_batch）          | 单页（folio）                     |
| 主要目标       | 拷贝数据到用户空间             | 建立虚拟地址到物理页的映射        |
| 是否涉及页表   | 否                            | 是                                |
| 是否需要锁页表 | 否                            | 需要                              |
| 是否考虑major fault | 否                        | 需要统计                          |
| 是否预读       | 支持，且更积极                 | 也支持，但更保守                  |
| 错误返回       | 已读字节数或负错误码           | VM_FAULT_XXX 标志                 |
| 典型调用者     | vfs_read/generic_file_read_iter| do_page_fault/mmap/vm_ops         |

---

## 四、简单总结

- **filemap_read**：用于普通文件读，批量处理，目标是把数据拷贝到用户空间。
- **filemap_fault**：用于 mmap 缺页异常，单页处理，目标是建立虚拟内存映射，保证进程能访问到文件内容。

两者都依赖页缓存，但服务的内核子系统和调用场景不同，处理细节和关注点也不同。

如需更细致的流程图或代码级对比，可以继续提问！

---

**User**

二者在触发同步预读上有什么异同，请你给出关键代码或者流程比较

---

**Cursor**

非常好的问题！下面用中文详细对比 `filemap_read` 和 `filemap_fault` 在**触发同步预读（readahead）**上的异同，并给出关键代码和流程。

---

## 一、filemap_read 的同步预读触发

### 1. 关键流程

在 `filemap_read` → `filemap_get_pages` 里，预读的触发逻辑如下：

```c
filemap_get_read_batch(mapping, index, last_index - 1, fbatch);
if (!folio_batch_count(fbatch)) {
    if (iocb->ki_flags & IOCB_NOIO)
        return -EAGAIN;
    page_cache_sync_readahead(mapping, ra, filp, index, last_index - index);
    filemap_get_read_batch(mapping, index, last_index - 1, fbatch);
}
```

**解释：**
- 首先尝试直接从页缓存批量获取页。
- 如果没有获取到（说明缓存里没有），并且允许IO，则调用 `page_cache_sync_readahead` 触发**同步预读**。
- 预读后再次尝试获取页。

### 2. 预读参数

- 预读的起始页是 `index`，长度是 `last_index - index`，即本次要读的页范围。
- 预读的行为由 `file_ra_state`（ra）控制，支持顺序读、随机读等优化。

---

## 二、filemap_fault 的同步预读触发

### 1. 关键流程

在 `filemap_fault` 里，预读的触发逻辑如下（以 do_sync_mmap_readahead 为例）：

```c
fpin = do_sync_mmap_readahead(vmf);
// ...
folio = __filemap_get_folio(mapping, index, FGP_CREAT|FGP_FOR_MMAP, vmf->gfp_mask);
```

`do_sync_mmap_readahead` 的实现：

```c
static struct file *do_sync_mmap_readahead(struct vm_fault *vmf)
{
    struct file *file = vmf->vma->vm_file;
    struct file_ra_state *ra = &file->f_ra;
    struct address_space *mapping = file->f_mapping;
    DEFINE_READAHEAD(ractl, file, ra, mapping, vmf->pgoff);
    // ... 省略部分判断 ...
    if (vm_flags & VM_SEQ_READ) {
        fpin = maybe_unlock_mmap_for_io(vmf, fpin);
        page_cache_sync_ra(&ractl, ra->ra_pages);
        return fpin;
    }
    // ... 省略 ...
    fpin = maybe_unlock_mmap_for_io(vmf, fpin);
    ra->start = max_t(long, 0, vmf->pgoff - ra->ra_pages / 2);
    ra->size = ra->ra_pages;
    ra->async_size = ra->ra_pages / 4;
    ractl._index = ra->start;
    page_cache_ra_order(&ractl, ra, 0);
    return fpin;
}
```

**解释：**
- 如果 mmap 访问模式是顺序读（`VM_SEQ_READ`），则直接以当前页为起点，长度为 `ra->ra_pages` 触发同步预读。
- 否则会以当前页为中心，向前向后都预读一部分。
- 预读的行为同样由 `file_ra_state` 控制。

---

## 三、异同点对比

| 对比点         | filemap_read（普通读）         | filemap_fault（缺页异常）         |
|----------------|-------------------------------|-----------------------------------|
| 触发时机       | 页缓存没有数据时，且允许IO     | 页缓存没有数据时，且允许IO        |
| 触发函数       | `page_cache_sync_readahead`    | `do_sync_mmap_readahead` → `page_cache_sync_ra` 或 `page_cache_ra_order` |
| 预读起点       | 当前读指针所在页               | 当前缺页的页（vmf->pgoff）        |
| 预读长度       | 本次要读的页数                 | 由 `ra->ra_pages` 决定，可能更大  |
| 预读策略       | 依赖 `file_ra_state`，可顺序/随机 | 依赖 `file_ra_state`，但 mmap 下更保守，miss 多时会关闭预读 |
| 预读后行为     | 立即再次尝试获取页             | 预读后进入新建页/读入流程         |
| 预读优化       | 更积极，适合大块顺序读         | 更保守，防止无用预读               |

---

## 四、关键代码对照

### filemap_read 触发同步预读

```c
// filemap_get_pages
filemap_get_read_batch(...);
if (!folio_batch_count(fbatch)) {
    page_cache_sync_readahead(...);
    filemap_get_read_batch(...);
}
```

### filemap_fault 触发同步预读

```c
// filemap_fault
fpin = do_sync_mmap_readahead(vmf);
// do_sync_mmap_readahead
page_cache_sync_ra(&ractl, ra->ra_pages);
// 或
page_cache_ra_order(&ractl, ra, 0);
```

---

## 五、总结

- **相同点**：两者都在页缓存没有数据时，且允许IO的情况下，触发同步预读，预读的行为都由 `file_ra_state` 控制。
- **不同点**：
  - filemap_read 预读更积极，直接以本次要读的范围为主，适合大块顺序读。
  - filemap_fault 预读更保守，会根据 mmap 访问模式和 miss 统计动态调整，防止无用预读，且有时会以当前页为中心向前后扩展。
  - filemap_fault 还会考虑 mmap 锁、major fault 统计等特殊场景。

---

如需更详细的代码分析或流程图，欢迎继续提问！

```c
filemap_get_read_batch(mapping, index, last_index - 1, fbatch);
if (!folio_batch_count(fbatch)) {
    if (iocb->ki_flags & IOCB_NOIO)
        return -EAGAIN;
    page_cache_sync_readahead(mapping, ra, filp, index, last_index - index);
    filemap_get_read_batch(mapping, index, last_index - 1, fbatch);
}
```

```c
fpin = do_sync_mmap_readahead(vmf);
// ...
folio = __filemap_get_folio(mapping, index, FGP_CREAT|FGP_FOR_MMAP, vmf->gfp_mask);
```

```c
static struct file *do_sync_mmap_readahead(struct vm_fault *vmf)
{
    struct file *file = vmf->vma->vm_file;
    struct file_ra_state *ra = &file->f_ra;
    struct address_space *mapping = file->f_mapping;
    DEFINE_READAHEAD(ractl, file, ra, mapping, vmf->pgoff);
    // ... 省略部分判断 ...
    if (vm_flags & VM_SEQ_READ) {
        fpin = maybe_unlock_mmap_for_io(vmf, fpin);
        page_cache_sync_ra(&ractl, ra->ra_pages);
        return fpin;
    }
    // ... 省略 ...
    fpin = maybe_unlock_mmap_for_io(vmf, fpin);
    ra->start = max_t(long, 0, vmf->pgoff - ra->ra_pages / 2);
    ra->size = ra->ra_pages;
    ra->async_size = ra->ra_pages / 4;
    ractl._index = ra->start;
    page_cache_ra_order(&ractl, ra, 0);
    return fpin;
}
```

```c
// filemap_get_pages
filemap_get_read_batch(...);
if (!folio_batch_count(fbatch)) {
    page_cache_sync_readahead(...);
    filemap_get_read_batch(...);
}
```

```c
// filemap_fault
fpin = do_sync_mmap_readahead(vmf);
// do_sync_mmap_readahead
page_cache_sync_ra(&ractl, ra->ra_pages);
// 或
page_cache_ra_order(&ractl, ra, 0);
```

---

**User**

他们会共同调用哪些接口

---

**Cursor**

非常好的问题！  
`filemap_read` 和 `filemap_fault` 在触发同步预读（readahead）时，**确实会共同调用一些底层的接口**。下面用中文详细说明它们的共同调用链和接口。

---

## 一、两者的预读调用链

### 1. filemap_read 相关调用链

- `filemap_read`
  - `filemap_get_pages`
    - `page_cache_sync_readahead(mapping, ra, filp, index, last_index - index)`

### 2. filemap_fault 相关调用链

- `filemap_fault`
  - `do_sync_mmap_readahead(vmf)`
    - `page_cache_sync_ra(&ractl, ra->ra_pages)`  
      或  
    - `page_cache_ra_order(&ractl, ra, 0)`

---

## 二、底层的共同接口

### 1. page_cache_sync_readahead

- **定义**：`mm/readahead.c`
- **作用**：同步触发一段范围的预读，适用于普通文件读和 mmap 缺页。
- **原型**：
  ```c
  void page_cache_sync_readahead(struct address_space *mapping,
                                 struct file_ra_state *ra,
                                 struct file *filp,
                                 pgoff_t offset,
                                 unsigned long req_size);
  ```

### 2. page_cache_sync_ra

- **定义**：`mm/readahead.c`
- **作用**：同步触发以当前页为起点、指定长度的预读。
- **原型**：
  ```c
  void page_cache_sync_ra(struct readahead_control *ractl, unsigned long nr_to_read);
  ```

### 3. page_cache_ra_order

- **定义**：`mm/readahead.c`
- **作用**：以指定 order（页块大小）进行预读，适用于大页场景。
- **原型**：
  ```c
  void page_cache_ra_order(struct readahead_control *ractl, struct file_ra_state *ra, unsigned int order);
  ```

---

## 三、它们的共同点

- **最终都依赖于 readahead 相关的核心实现**，如 `page_cache_sync_readahead`、`page_cache_sync_ra`、`page_cache_ra_order`。
- 这些接口会进一步调用更底层的页分配、IO 提交等函数（如 `read_pages`、`do_page_cache_readahead` 等）。

---

## 四、代码举例

### filemap_get_pages 里

```c
page_cache_sync_readahead(mapping, ra, filp, index, last_index - index);
```

### do_sync_mmap_readahead 里

```c
page_cache_sync_ra(&ractl, ra->ra_pages);
// 或
page_cache_ra_order(&ractl, ra, 0);
```

---

## 五、总结

**filemap_read 和 filemap_fault 在触发同步预读时，都会调用如下核心接口：**

- `page_cache_sync_readahead`
- `page_cache_sync_ra`
- `page_cache_ra_order`

这些接口共同承担了“根据访问模式和需求，批量预读文件页到缓存”的功能，是 Linux 页缓存预读机制的核心 API。

---

如需进一步了解这些接口的实现细节或调用关系，可以继续提问！

```c
  void page_cache_sync_readahead(struct address_space *mapping,
                                 struct file_ra_state *ra,
                                 struct file *filp,
                                 pgoff_t offset,
                                 unsigned long req_size);
```

```c
  void page_cache_sync_ra(struct readahead_control *ractl, unsigned long nr_to_read);
```

```c
  void page_cache_ra_order(struct readahead_control *ractl, struct file_ra_state *ra, unsigned int order);
```

```c
page_cache_sync_readahead(mapping, ra, filp, index, last_index - index);
```

```c
page_cache_sync_ra(&ractl, ra->ra_pages);
// 或
page_cache_ra_order(&ractl, ra, 0);
```

---

**User**

这个函数的含义是什么？

---

**Cursor**

这个函数的作用是：**判断当前块设备 cgroup（blkcg）或其任意父 cgroup 是否处于“拥塞（congested）”状态**。

---

### 详细解释

#### 1. 代码流程

```c
bool blk_cgroup_congested(void)
{
	struct cgroup_subsys_state *css;
	bool ret = false;

	rcu_read_lock();
	for (css = blkcg_css(); css; css = css->parent) {
		if (atomic_read(&css->cgroup->congestion_count)) {
			ret = true;
			break;
		}
	}
	rcu_read_unlock();
	return ret;
}
```

- `blkcg_css()`：获取当前任务所属的块设备 cgroup 的状态（css）。
- `for (css = blkcg_css(); css; css = css->parent)`：从当前 cgroup 开始，逐级向上遍历所有父 cgroup。
- `if (atomic_read(&css->cgroup->congestion_count))`：如果当前 cgroup 的 `congestion_count`（拥塞计数）大于 0，说明该 cgroup 处于拥塞状态。
- 如果发现有拥塞，`ret = true`，并跳出循环。
- 最后返回 `ret`，表示是否有拥塞。

#### 2. 关键字段

- `congestion_count`：表示该 cgroup 下的块设备是否因为 IO 压力等原因被标记为“拥塞”。
- 拥塞状态通常意味着该 cgroup 的 IO 处理能力已达到瓶颈，可能需要限流或等待。

#### 3. 应用场景

- 该函数常用于判断是否需要对 IO 操作进行限速、等待或做特殊处理。
- 例如，内核在写回脏页、分配缓存等场景下，会根据拥塞状态决定是否要 sleep、重试等。

---

### 总结

**blk_cgroup_congested()**  
就是用来判断“当前任务所属的块设备 cgroup 及其所有父 cgroup 是否有被标记为拥塞（congested）”。  
如果有任意一级 cgroup 处于拥塞状态，则返回 true，否则返回 false。

---

如需了解“拥塞”具体如何产生、如何清除，或 cgroup 相关机制，可以继续提问！

```c
bool blk_cgroup_congested(void)
{
	struct cgroup_subsys_state *css;
	bool ret = false;

	rcu_read_lock();
	for (css = blkcg_css(); css; css = css->parent) {
		if (atomic_read(&css->cgroup->congestion_count)) {
			ret = true;
			break;
		}
	}
	rcu_read_unlock();
	return ret;
}
```

---

**User**

请你解读的这个函数的逻辑，使用自然语言配合相关函数调用解答

---

**Cursor**

好的，下面我用中文结合关键函数调用，详细解读 `filemap_fault` 这个函数的整体逻辑和处理流程。

---

## 1. 作用概述

`filemap_fault` 是 Linux 内核中**处理文件映射（mmap）缺页异常**的核心函数。当进程访问通过 mmap 映射的文件区域、但对应的物理页还未分配或未加载到内存时，内核会调用该函数来“补齐”所需的页。

---

## 2. 主要流程分解

### 1）参数和变量准备

- 通过 `vmf`（`struct vm_fault *`）获取当前缺页的文件、映射、inode、页号等信息。
- 计算当前文件的最大页号 `max_idx`，以及本次缺页的页号 `index`。

### 2）边界检查

- 如果本次缺页的页号超出了文件实际大小，直接返回 `VM_FAULT_SIGBUS`，表示非法访问。

### 3）尝试在页缓存中查找目标页

- 调用 `filemap_get_folio(mapping, index)`，在页缓存（page cache）中查找目标页（folio）。
- 如果找到了（不是错误指针）：
  - 如果还没尝试过异步预读（`FAULT_FLAG_TRIED` 没有设置），调用 `do_async_mmap_readahead(vmf, folio)` 进行异步预读优化。
  - 如果该页不是 uptodate（数据还没准备好），需要加 `invalidate_lock`，防止与截断等操作冲突。

- 如果没找到（返回错误指针）：
  - 统计 major fault 事件（`count_vm_event(PGMAJFAULT)`）。
  - 触发同步预读（`do_sync_mmap_readahead(vmf)`），提前把后续可能要用到的页读入缓存。
  - 进入 `retry_find` 流程，准备新建页。

### 4）新建页并读入数据（retry_find）

- 如果还没加 `invalidate_lock`，现在加锁，保证安全。
- 调用 `__filemap_get_folio(mapping, index, FGP_CREAT|FGP_FOR_MMAP, vmf->gfp_mask)`，尝试新建一个 folio 并插入页缓存。
- 如果新建失败（返回错误指针）：
  - 如果之前有预读文件指针（`fpin`），跳转到 `out_retry`，让上层重试。
  - 否则解锁并返回 `VM_FAULT_OOM`，表示内存不足。

### 5）加锁并检查页状态

- 调用 `lock_folio_maybe_drop_mmap(vmf, folio, &fpin)`，尝试加锁该 folio。如果需要释放 mmap_lock，则跳转到 `out_retry`，让上层重试。
- 检查该页是否被截断（`folio->mapping != mapping`），如果是，解锁并释放，重新查找。

### 6）确保页数据是最新的

- 如果该页不是 uptodate（数据还没准备好）：
  - 如果之前没加锁，解锁并重新查找。
  - 否则进入 `page_not_uptodate`，同步读取数据。

#### page_not_uptodate 流程

- 释放 mmap_lock（如果需要）。
- 调用 `filemap_read_folio(file, mapping->a_ops->read_folio, folio)`，同步从底层读取数据到该页。
- 如果需要重试（如页被截断），重新查找。
- 如果出错，解锁并返回 `VM_FAULT_SIGBUS`。

### 7）完成映射

- 如果一切顺利，且 mmap_lock 没被释放，解锁 `invalidate_lock`。
- 再次检查页号是否越界。
- 将该页插入到进程的页表（`vmf->page = folio_file_page(folio, index)`）。
- 返回 `ret | VM_FAULT_LOCKED`，表示缺页处理成功，且页已加锁。

### 8）重试和清理（out_retry）

- 如果需要重试，释放引用、解锁、关闭文件指针等，返回 `ret | VM_FAULT_RETRY`，让上层重新查找 VMA 并重试缺页处理。

---

## 3. 关键函数说明

- **filemap_get_folio**：在页缓存中查找目标页，如果没有则返回错误指针。
- **do_async_mmap_readahead / do_sync_mmap_readahead**：分别用于异步和同步预读，优化顺序访问性能。
- **filemap_invalidate_lock_shared / filemap_invalidate_unlock_shared**：加/解 invalidate_lock，防止与截断等操作冲突。
- **__filemap_get_folio**：如果页不存在，则新建页并插入页缓存。
- **lock_folio_maybe_drop_mmap**：尝试加锁 folio，必要时释放 mmap_lock。
- **filemap_read_folio**：同步从底层读取数据到 folio。
- **folio_file_page**：获取 folio 对应的 struct page 指针，用于插入页表。

---

## 4. 总结

**filemap_fault** 的整体逻辑就是：
1. 检查缺页位置是否合法。
2. 优先尝试从页缓存获取目标页，必要时触发预读。
3. 如果页不存在，则新建页并从底层读取数据。
4. 确保页数据是最新的，且加锁。
5. 将页插入进程页表，完成缺页处理。
6. 处理过程中注意各种锁的获取与释放，保证并发安全。
7. 如果遇到需要重试的情况，优雅地返回，让上层重新查找和处理。

---

如需对某个分支或子函数进一步详细解释，欢迎继续提问！

