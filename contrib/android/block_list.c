#include "block_list.h"
#include "block_range.h"
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

struct block_list {
	FILE *f;
	const char *mountpoint;

	struct {
		const char *filename;
		struct block_range *head;
		struct block_range *tail;
	} entry;
};

static void *init(const char *file, const char *mountpoint)
{
	struct block_list *params = malloc(sizeof(*params));

	if (!params)
		return NULL;
	params->mountpoint = mountpoint;
	params->f = fopen(file, "w+");
	if (!params->f) {
		free(params);
		return NULL;
	}
	return params;
}

static int start_new_file(char *path, ext2_ino_t ino EXT2FS_ATTR((unused)),
			  struct ext2_inode *inode EXT2FS_ATTR((unused)),
			  void *data)
{
	struct block_list *params = data;

	params->entry.head = params->entry.tail = NULL;
	params->entry.filename = LINUX_S_ISREG(inode->i_mode) ? path : NULL;
	return 0;
}

static int add_block(ext2_filsys fs EXT2FS_ATTR((unused)), blk64_t blocknr,
		     int metadata, void *data)
{
	struct block_list *params = data;

	if (params->entry.filename && !metadata)
		add_blocks_to_range(&params->entry.head, &params->entry.tail,
				    blocknr, blocknr);
	return 0;
}

static int inline_data(void *inline_data EXT2FS_ATTR((unused)),
		       void *data EXT2FS_ATTR((unused)))
{
	return 0;
}

static int end_new_file(void *data)
{
	struct block_list *params = data;

	if (!params->entry.filename || !params->entry.head)
		return 0;
	if (fprintf(params->f, "%s%s ", params->mountpoint,
		    params->entry.filename) < 0
	    || write_block_ranges(params->f, params->entry.head, " ")
	    || fwrite("\n", 1, 1, params->f) != 1)
		return -1;

	delete_block_ranges(params->entry.head);
	return 0;
}

static int cleanup(void *data)
{
	struct block_list *params = data;

	fclose(params->f);
	free(params);
	return 0;
}

struct fsmap_format block_list_format = {
	.init = init,
	.start_new_file = start_new_file,
	.add_block = add_block,
	.inline_data = inline_data,
	.end_new_file = end_new_file,
	.cleanup = cleanup,
};
