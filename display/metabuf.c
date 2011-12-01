#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "display.h"
#include "buffer.h"

PSMETABUFER
metabuf_alloc()
{
	PSMETABUFER res = malloc(sizeof(SMETABUFER));
	memset(res, 0, sizeof(SMETABUFER));
	return res;
}

void
metabuf_free(PSMETABUFER metabuf)
{
	if (metabuf->left)
		buf_free(metabuf->left);

	if (metabuf->right)
		buf_free(metabuf->right);

	if (metabuf->left_fft)
		buf_free(metabuf->left_fft);

	if (metabuf->right_fft)
		buf_free(metabuf->right_fft);

	if (metabuf->sd_log)
		buf_free(metabuf->sd_log);

	if (metabuf->sd_mod)
		buf_free(metabuf->sd_mod);

	free(metabuf);
}
