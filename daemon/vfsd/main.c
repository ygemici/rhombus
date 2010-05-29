/* 
 * Copyright 2009, 2010 Nick Johnson 
 * ISC Licensed, see LICENSE for details
 */

#include <flux/arch.h>
#include <flux/ipc.h>
#include <flux/packet.h>
#include <flux/proc.h>
#include <flux/driver.h>
#include <flux/vfs.h>

#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include <vfsd.h>

struct vfs *vfs_root;
uint32_t m_vfs;

static char *path_preprocess(char *path) {
	char buffer[1000];
	size_t i, j;

	i = 0;

	buffer[0] = '/';
	j = 1;

	for (;path[i] == '/'; i++);

	for (; path[i]; i++, j++) {
		if (!(path[i] == '/' && path[i+1] == '/')) {
			buffer[j] = path[i];
		}
	}

	if (path[i-1] == '/') {
		buffer[j-1] = '\0';
	}
	else {
		buffer[j] = '\0';
	}

	strcpy(path, buffer);

	return path;
}

void vfs_handle(uint32_t caller, struct packet *packet) {
	struct vfs_query *q;
	struct vfs *file;

	if (!packet) {
		packet = packet_alloc(0);
		send(PORT_ERROR, caller, packet);
		packet_free(packet);
		return;
	}

	q = packet_getbuf(packet);

	switch (q->command) {
	case VFS_CMD_FIND:
		mutex_spin(&m_vfs);
		file = vfs_get(vfs_root, path_preprocess(q->path0));
		mutex_free(&m_vfs);

		if (file) {
			q->server = file->server;
			q->inode  = file->inode;
		}
		else {
			packet->data_length = 0;
		}
		break;
	case VFS_CMD_ADD:
		mutex_spin(&m_vfs);
		vfs_add(vfs_root, path_preprocess(q->path0), q->server, q->inode);
		mutex_free(&m_vfs);
		break;
	}

	q->command = VFS_CMD_REPLY;
	send(PORT_REPLY, caller, packet);
	packet_free(packet);
}

int main() {

	when(PORT_QUERY, vfs_handle);

	mutex_spin(&m_vfs);
	vfs_root = calloc(sizeof(struct vfs), 1);
	mutex_free(&m_vfs);

	printf("vfsd: ready\n");

	send(PORT_SYNC, 1, NULL);

	_done();

	return 0;
}