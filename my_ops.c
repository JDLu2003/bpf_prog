// SPDX-License-Identifier: GPL-2.0
#include <linux/bpf.h>
#include <linux/btf.h>
#include <linux/btf_ids.h>
#include <linux/init.h>
#include <linux/module.h>
#include <linux/bpf_struct_ops.h>

struct my_ops {
	int (*my_custom_func)(const char *filename);
};

static struct bpf_struct_ops my_ops_struct_ops;  // BPF 注册时使用

/* BTF ID 表，内核在加载时匹配 */
static const struct btf_type *my_ops_btf_type;

static int my_ops_init(struct btf *btf)
{
	my_ops_btf_type = btf_find_by_name_kind(btf, "my_ops", BTF_KIND_STRUCT);
	if (!my_ops_btf_type)
		return -EINVAL;
	return 0;
}

static int my_ops_check_member(const struct btf_type *t, const char *name,
			       const struct btf_member *member)
{
	/* 可根据需要检查成员，这里允许全部通过 */
	return 0;
}

static int my_ops_init_member(const struct btf_type *t, const char *name,
			      void *kdata, const void *udata)
{
	/* 注册时初始化，这里直接使用用户传入的 trampoline */
	return 0;
}

static void my_ops_exit(void *kdata)
{
	/* 注销时清理逻辑，可空 */
}

static int my_ops_get_member(const void *kdata, const char *name, void *buf,
			     u32 size)
{
	return -EOPNOTSUPP;
}

static struct bpf_struct_ops my_ops_struct_ops = {
	.verifier_ops = {
		.init		= my_ops_init,
		.check_member	= my_ops_check_member,
		.init_member	= my_ops_init_member,
		.exit		= my_ops_exit,
		.get_member	= my_ops_get_member,
	},
	.name = "my_ops",
};

static int __init my_ops_init_module(void)
{
	return register_bpf_struct_ops(&my_ops_struct_ops);
}

static void __exit my_ops_exit_module(void)
{
	unregister_bpf_struct_ops(&my_ops_struct_ops);
}

module_init(my_ops_init_module);
module_exit(my_ops_exit_module);
MODULE_LICENSE("GPL");
