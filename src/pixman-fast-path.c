/* -*- Mode: c; c-basic-offset: 4; tab-width: 8; indent-tabs-mode: t; -*- */
/*
 * Copyright © 2000 SuSE, Inc.
 * Copyright © 2007 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of SuSE not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  SuSE makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * SuSE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL SuSE
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Author:  Keith Packard, SuSE, Inc.
 */

#ifdef HAVE_CONFIG_H
#include "./config.h"
#endif
#include <xClib/string.h>
#include <xC/xmemory.h>
#include "pixman-private.h"
#include "pixman-combine32.h"
#include "pixman-inlines.h"

static force_inline xuint32_t
fetch_24 (xuint8_t *a)
{
    if (((xuintptr_t)a) & 1)
    {
#ifdef WORDS_BIGENDIAN
	return (*a << 16) | (*(xuint16_t *)(a + 1));
#else
	return *a | (*(xuint16_t *)(a + 1) << 8);
#endif
    }
    else
    {
#ifdef WORDS_BIGENDIAN
	return (*(xuint16_t *)a << 8) | *(a + 2);
#else
	return *(xuint16_t *)a | (*(a + 2) << 16);
#endif
    }
}

static force_inline void
store_24 (xuint8_t *a,
          xuint32_t v)
{
    if (((xuintptr_t)a) & 1)
    {
#ifdef WORDS_BIGENDIAN
	*a = (xuint8_t) (v >> 16);
	*(xuint16_t *)(a + 1) = (xuint16_t) (v);
#else
	*a = (xuint8_t) (v);
	*(xuint16_t *)(a + 1) = (xuint16_t) (v >> 8);
#endif
    }
    else
    {
#ifdef WORDS_BIGENDIAN
	*(xuint16_t *)a = (xuint16_t)(v >> 8);
	*(a + 2) = (xuint8_t)v;
#else
	*(xuint16_t *)a = (xuint16_t)v;
	*(a + 2) = (xuint8_t)(v >> 16);
#endif
    }
}

static force_inline xuint32_t
over (xuint32_t src,
      xuint32_t dest)
{
    xuint32_t a = ~src >> 24;

    UN8x4_MUL_UN8_ADD_UN8x4 (dest, a, src);

    return dest;
}

static force_inline xuint32_t
in (xuint32_t x,
    xuint8_t  y)
{
    xuint16_t a = y;

    UN8x4_MUL_UN8 (x, a);

    return x;
}

/*
 * Naming convention:
 *
 *  op_src_mask_dest
 */
static void
fast_composite_over_x888_8_8888 (pixman_implementation_t *imp,
                                 pixman_composite_info_t *info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    xuint32_t    *src, *src_line;
    xuint32_t    *dst, *dst_line;
    xuint8_t     *mask, *mask_line;
    int src_stride, mask_stride, dst_stride;
    xuint8_t m;
    xuint32_t s, d;
    xint32_t w;

    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, xuint32_t, dst_stride, dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (mask_image, mask_x, mask_y, xuint8_t, mask_stride, mask_line, 1);
    PIXMAN_IMAGE_GET_LINE (src_image, src_x, src_y, xuint32_t, src_stride, src_line, 1);

    while (height--)
    {
	src = src_line;
	src_line += src_stride;
	dst = dst_line;
	dst_line += dst_stride;
	mask = mask_line;
	mask_line += mask_stride;

	w = width;
	while (w--)
	{
	    m = *mask++;
	    if (m)
	    {
		s = *src | 0xff000000;

		if (m == 0xff)
		{
		    *dst = s;
		}
		else
		{
		    d = in (s, m);
		    *dst = over (d, *dst);
		}
	    }
	    src++;
	    dst++;
	}
    }
}

static void
fast_composite_in_n_8_8 (pixman_implementation_t *imp,
                         pixman_composite_info_t *info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    xuint32_t src, srca;
    xuint8_t     *dst_line, *dst;
    xuint8_t     *mask_line, *mask, m;
    int dst_stride, mask_stride;
    xint32_t w;
    xuint16_t t;

    src = _pixman_image_get_solid (imp, src_image, dest_image->bits.format);

    srca = src >> 24;

    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, xuint8_t, dst_stride, dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (mask_image, mask_x, mask_y, xuint8_t, mask_stride, mask_line, 1);

    if (srca == 0xff)
    {
	while (height--)
	{
	    dst = dst_line;
	    dst_line += dst_stride;
	    mask = mask_line;
	    mask_line += mask_stride;
	    w = width;

	    while (w--)
	    {
		m = *mask++;

		if (m == 0)
		    *dst = 0;
		else if (m != 0xff)
		    *dst = MUL_UN8 (m, *dst, t);

		dst++;
	    }
	}
    }
    else
    {
	while (height--)
	{
	    dst = dst_line;
	    dst_line += dst_stride;
	    mask = mask_line;
	    mask_line += mask_stride;
	    w = width;

	    while (w--)
	    {
		m = *mask++;
		m = MUL_UN8 (m, srca, t);

		if (m == 0)
		    *dst = 0;
		else if (m != 0xff)
		    *dst = MUL_UN8 (m, *dst, t);

		dst++;
	    }
	}
    }
}

static void
fast_composite_in_8_8 (pixman_implementation_t *imp,
                       pixman_composite_info_t *info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    xuint8_t     *dst_line, *dst;
    xuint8_t     *src_line, *src;
    int dst_stride, src_stride;
    xint32_t w;
    xuint8_t s;
    xuint16_t t;

    PIXMAN_IMAGE_GET_LINE (src_image, src_x, src_y, xuint8_t, src_stride, src_line, 1);
    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, xuint8_t, dst_stride, dst_line, 1);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;
	w = width;

	while (w--)
	{
	    s = *src++;

	    if (s == 0)
		*dst = 0;
	    else if (s != 0xff)
		*dst = MUL_UN8 (s, *dst, t);

	    dst++;
	}
    }
}

static void
fast_composite_over_n_8_8888 (pixman_implementation_t *imp,
                              pixman_composite_info_t *info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    xuint32_t src, srca;
    xuint32_t    *dst_line, *dst, d;
    xuint8_t     *mask_line, *mask, m;
    int dst_stride, mask_stride;
    xint32_t w;

    src = _pixman_image_get_solid (imp, src_image, dest_image->bits.format);

    srca = src >> 24;
    if (src == 0)
	return;

    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, xuint32_t, dst_stride, dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (mask_image, mask_x, mask_y, xuint8_t, mask_stride, mask_line, 1);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	mask = mask_line;
	mask_line += mask_stride;
	w = width;

	while (w--)
	{
	    m = *mask++;
	    if (m == 0xff)
	    {
		if (srca == 0xff)
		    *dst = src;
		else
		    *dst = over (src, *dst);
	    }
	    else if (m)
	    {
		d = in (src, m);
		*dst = over (d, *dst);
	    }
	    dst++;
	}
    }
}

static void
fast_composite_add_n_8888_8888_ca (pixman_implementation_t *imp,
				   pixman_composite_info_t *info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    xuint32_t src, s;
    xuint32_t    *dst_line, *dst, d;
    xuint32_t    *mask_line, *mask, ma;
    int dst_stride, mask_stride;
    xint32_t w;

    src = _pixman_image_get_solid (imp, src_image, dest_image->bits.format);

    if (src == 0)
	return;

    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, xuint32_t, dst_stride, dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (mask_image, mask_x, mask_y, xuint32_t, mask_stride, mask_line, 1);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	mask = mask_line;
	mask_line += mask_stride;
	w = width;

	while (w--)
	{
	    ma = *mask++;

	    if (ma)
	    {
		d = *dst;
		s = src;

		UN8x4_MUL_UN8x4_ADD_UN8x4 (s, ma, d);

		*dst = s;
	    }

	    dst++;
	}
    }
}

static void
fast_composite_over_n_8888_8888_ca (pixman_implementation_t *imp,
                                    pixman_composite_info_t *info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    xuint32_t src, srca, s;
    xuint32_t    *dst_line, *dst, d;
    xuint32_t    *mask_line, *mask, ma;
    int dst_stride, mask_stride;
    xint32_t w;

    src = _pixman_image_get_solid (imp, src_image, dest_image->bits.format);

    srca = src >> 24;
    if (src == 0)
	return;

    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, xuint32_t, dst_stride, dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (mask_image, mask_x, mask_y, xuint32_t, mask_stride, mask_line, 1);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	mask = mask_line;
	mask_line += mask_stride;
	w = width;

	while (w--)
	{
	    ma = *mask++;
	    if (ma == 0xffffffff)
	    {
		if (srca == 0xff)
		    *dst = src;
		else
		    *dst = over (src, *dst);
	    }
	    else if (ma)
	    {
		d = *dst;
		s = src;

		UN8x4_MUL_UN8x4 (s, ma);
		UN8x4_MUL_UN8 (ma, srca);
		ma = ~ma;
		UN8x4_MUL_UN8x4_ADD_UN8x4 (d, ma, s);

		*dst = d;
	    }

	    dst++;
	}
    }
}

static void
fast_composite_over_n_8_0888 (pixman_implementation_t *imp,
                              pixman_composite_info_t *info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    xuint32_t src, srca;
    xuint8_t     *dst_line, *dst;
    xuint32_t d;
    xuint8_t     *mask_line, *mask, m;
    int dst_stride, mask_stride;
    xint32_t w;

    src = _pixman_image_get_solid (imp, src_image, dest_image->bits.format);

    srca = src >> 24;
    if (src == 0)
	return;

    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, xuint8_t, dst_stride, dst_line, 3);
    PIXMAN_IMAGE_GET_LINE (mask_image, mask_x, mask_y, xuint8_t, mask_stride, mask_line, 1);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	mask = mask_line;
	mask_line += mask_stride;
	w = width;

	while (w--)
	{
	    m = *mask++;
	    if (m == 0xff)
	    {
		if (srca == 0xff)
		{
		    d = src;
		}
		else
		{
		    d = fetch_24 (dst);
		    d = over (src, d);
		}
		store_24 (dst, d);
	    }
	    else if (m)
	    {
		d = over (in (src, m), fetch_24 (dst));
		store_24 (dst, d);
	    }
	    dst += 3;
	}
    }
}

static void
fast_composite_over_n_8_0565 (pixman_implementation_t *imp,
                              pixman_composite_info_t *info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    xuint32_t src, srca;
    xuint16_t    *dst_line, *dst;
    xuint32_t d;
    xuint8_t     *mask_line, *mask, m;
    int dst_stride, mask_stride;
    xint32_t w;

    src = _pixman_image_get_solid (imp, src_image, dest_image->bits.format);

    srca = src >> 24;
    if (src == 0)
	return;

    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, xuint16_t, dst_stride, dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (mask_image, mask_x, mask_y, xuint8_t, mask_stride, mask_line, 1);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	mask = mask_line;
	mask_line += mask_stride;
	w = width;

	while (w--)
	{
	    m = *mask++;
	    if (m == 0xff)
	    {
		if (srca == 0xff)
		{
		    d = src;
		}
		else
		{
		    d = *dst;
		    d = over (src, convert_0565_to_0888 (d));
		}
		*dst = convert_8888_to_0565 (d);
	    }
	    else if (m)
	    {
		d = *dst;
		d = over (in (src, m), convert_0565_to_0888 (d));
		*dst = convert_8888_to_0565 (d);
	    }
	    dst++;
	}
    }
}

static void
fast_composite_over_n_8888_0565_ca (pixman_implementation_t *imp,
                                    pixman_composite_info_t *info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    xuint32_t  src, srca, s;
    xuint16_t  src16;
    xuint16_t *dst_line, *dst;
    xuint32_t  d;
    xuint32_t *mask_line, *mask, ma;
    int dst_stride, mask_stride;
    xint32_t w;

    src = _pixman_image_get_solid (imp, src_image, dest_image->bits.format);

    srca = src >> 24;
    if (src == 0)
	return;

    src16 = convert_8888_to_0565 (src);

    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, xuint16_t, dst_stride, dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (mask_image, mask_x, mask_y, xuint32_t, mask_stride, mask_line, 1);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	mask = mask_line;
	mask_line += mask_stride;
	w = width;

	while (w--)
	{
	    ma = *mask++;
	    if (ma == 0xffffffff)
	    {
		if (srca == 0xff)
		{
		    *dst = src16;
		}
		else
		{
		    d = *dst;
		    d = over (src, convert_0565_to_0888 (d));
		    *dst = convert_8888_to_0565 (d);
		}
	    }
	    else if (ma)
	    {
		d = *dst;
		d = convert_0565_to_0888 (d);

		s = src;

		UN8x4_MUL_UN8x4 (s, ma);
		UN8x4_MUL_UN8 (ma, srca);
		ma = ~ma;
		UN8x4_MUL_UN8x4_ADD_UN8x4 (d, ma, s);

		*dst = convert_8888_to_0565 (d);
	    }
	    dst++;
	}
    }
}

static void
fast_composite_over_8888_8888 (pixman_implementation_t *imp,
                               pixman_composite_info_t *info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    xuint32_t    *dst_line, *dst;
    xuint32_t    *src_line, *src, s;
    int dst_stride, src_stride;
    xuint8_t a;
    xint32_t w;

    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, xuint32_t, dst_stride, dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (src_image, src_x, src_y, xuint32_t, src_stride, src_line, 1);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;
	w = width;

	while (w--)
	{
	    s = *src++;
	    a = s >> 24;
	    if (a == 0xff)
		*dst = s;
	    else if (s)
		*dst = over (s, *dst);
	    dst++;
	}
    }
}

static void
fast_composite_src_x888_8888 (pixman_implementation_t *imp,
			      pixman_composite_info_t *info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    xuint32_t    *dst_line, *dst;
    xuint32_t    *src_line, *src;
    int dst_stride, src_stride;
    xint32_t w;

    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, xuint32_t, dst_stride, dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (src_image, src_x, src_y, xuint32_t, src_stride, src_line, 1);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;
	w = width;

	while (w--)
	    *dst++ = (*src++) | 0xff000000;
    }
}

#if 0
static void
fast_composite_over_8888_0888 (pixman_implementation_t *imp,
			       pixman_composite_info_t *info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    xuint8_t     *dst_line, *dst;
    xuint32_t d;
    xuint32_t    *src_line, *src, s;
    xuint8_t a;
    int dst_stride, src_stride;
    xint32_t w;

    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, xuint8_t, dst_stride, dst_line, 3);
    PIXMAN_IMAGE_GET_LINE (src_image, src_x, src_y, xuint32_t, src_stride, src_line, 1);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;
	w = width;

	while (w--)
	{
	    s = *src++;
	    a = s >> 24;
	    if (a)
	    {
		if (a == 0xff)
		    d = s;
		else
		    d = over (s, fetch_24 (dst));

		store_24 (dst, d);
	    }
	    dst += 3;
	}
    }
}
#endif

static void
fast_composite_over_8888_0565 (pixman_implementation_t *imp,
                               pixman_composite_info_t *info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    xuint16_t    *dst_line, *dst;
    xuint32_t d;
    xuint32_t    *src_line, *src, s;
    xuint8_t a;
    int dst_stride, src_stride;
    xint32_t w;

    PIXMAN_IMAGE_GET_LINE (src_image, src_x, src_y, xuint32_t, src_stride, src_line, 1);
    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, xuint16_t, dst_stride, dst_line, 1);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;
	w = width;

	while (w--)
	{
	    s = *src++;
	    a = s >> 24;
	    if (s)
	    {
		if (a == 0xff)
		{
		    d = s;
		}
		else
		{
		    d = *dst;
		    d = over (s, convert_0565_to_0888 (d));
		}
		*dst = convert_8888_to_0565 (d);
	    }
	    dst++;
	}
    }
}

static void
fast_composite_add_8_8 (pixman_implementation_t *imp,
			pixman_composite_info_t *info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    xuint8_t     *dst_line, *dst;
    xuint8_t     *src_line, *src;
    int dst_stride, src_stride;
    xint32_t w;
    xuint8_t s, d;
    xuint16_t t;

    PIXMAN_IMAGE_GET_LINE (src_image, src_x, src_y, xuint8_t, src_stride, src_line, 1);
    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, xuint8_t, dst_stride, dst_line, 1);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;
	w = width;

	while (w--)
	{
	    s = *src++;
	    if (s)
	    {
		if (s != 0xff)
		{
		    d = *dst;
		    t = d + s;
		    s = t | (0 - (t >> 8));
		}
		*dst = s;
	    }
	    dst++;
	}
    }
}

static void
fast_composite_add_0565_0565 (pixman_implementation_t *imp,
                              pixman_composite_info_t *info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    xuint16_t    *dst_line, *dst;
    xuint32_t	d;
    xuint16_t    *src_line, *src;
    xuint32_t	s;
    int dst_stride, src_stride;
    xint32_t w;

    PIXMAN_IMAGE_GET_LINE (src_image, src_x, src_y, xuint16_t, src_stride, src_line, 1);
    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, xuint16_t, dst_stride, dst_line, 1);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;
	w = width;

	while (w--)
	{
	    s = *src++;
	    if (s)
	    {
		d = *dst;
		s = convert_0565_to_8888 (s);
		if (d)
		{
		    d = convert_0565_to_8888 (d);
		    UN8x4_ADD_UN8x4 (s, d);
		}
		*dst = convert_8888_to_0565 (s);
	    }
	    dst++;
	}
    }
}

static void
fast_composite_add_8888_8888 (pixman_implementation_t *imp,
                              pixman_composite_info_t *info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    xuint32_t    *dst_line, *dst;
    xuint32_t    *src_line, *src;
    int dst_stride, src_stride;
    xint32_t w;
    xuint32_t s, d;

    PIXMAN_IMAGE_GET_LINE (src_image, src_x, src_y, xuint32_t, src_stride, src_line, 1);
    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, xuint32_t, dst_stride, dst_line, 1);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;
	w = width;

	while (w--)
	{
	    s = *src++;
	    if (s)
	    {
		if (s != 0xffffffff)
		{
		    d = *dst;
		    if (d)
			UN8x4_ADD_UN8x4 (s, d);
		}
		*dst = s;
	    }
	    dst++;
	}
    }
}

static void
fast_composite_add_n_8_8 (pixman_implementation_t *imp,
			  pixman_composite_info_t *info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    xuint8_t     *dst_line, *dst;
    xuint8_t     *mask_line, *mask;
    int dst_stride, mask_stride;
    xint32_t w;
    xuint32_t src;
    xuint8_t sa;

    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, xuint8_t, dst_stride, dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (mask_image, mask_x, mask_y, xuint8_t, mask_stride, mask_line, 1);
    src = _pixman_image_get_solid (imp, src_image, dest_image->bits.format);
    sa = (src >> 24);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	mask = mask_line;
	mask_line += mask_stride;
	w = width;

	while (w--)
	{
	    xuint16_t tmp;
	    xuint16_t a;
	    xuint32_t m, d;
	    xuint32_t r;

	    a = *mask++;
	    d = *dst;

	    m = MUL_UN8 (sa, a, tmp);
	    r = ADD_UN8 (m, d, tmp);

	    *dst++ = r;
	}
    }
}

#ifdef WORDS_BIGENDIAN
#define CREATE_BITMASK(n) (0x80000000 >> (n))
#define UPDATE_BITMASK(n) ((n) >> 1)
#else
#define CREATE_BITMASK(n) (1 << (n))
#define UPDATE_BITMASK(n) ((n) << 1)
#endif

#define TEST_BIT(p, n)					\
    (*((p) + ((n) >> 5)) & CREATE_BITMASK ((n) & 31))
#define SET_BIT(p, n)							\
    do { *((p) + ((n) >> 5)) |= CREATE_BITMASK ((n) & 31); } while (0);

static void
fast_composite_add_1_1 (pixman_implementation_t *imp,
			pixman_composite_info_t *info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    xuint32_t     *dst_line, *dst;
    xuint32_t     *src_line, *src;
    int           dst_stride, src_stride;
    xint32_t       w;

    PIXMAN_IMAGE_GET_LINE (src_image, 0, src_y, xuint32_t,
                           src_stride, src_line, 1);
    PIXMAN_IMAGE_GET_LINE (dest_image, 0, dest_y, xuint32_t,
                           dst_stride, dst_line, 1);

    while (height--)
    {
	dst = dst_line;
	dst_line += dst_stride;
	src = src_line;
	src_line += src_stride;
	w = width;

	while (w--)
	{
	    /*
	     * TODO: improve performance by processing xuint32_t data instead
	     *       of individual bits
	     */
	    if (TEST_BIT (src, src_x + w))
		SET_BIT (dst, dest_x + w);
	}
    }
}

static void
fast_composite_over_n_1_8888 (pixman_implementation_t *imp,
                              pixman_composite_info_t *info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    xuint32_t     src, srca;
    xuint32_t    *dst, *dst_line;
    xuint32_t    *mask, *mask_line;
    int          mask_stride, dst_stride;
    xuint32_t     bitcache, bitmask;
    xint32_t      w;

    if (width <= 0)
	return;

    src = _pixman_image_get_solid (imp, src_image, dest_image->bits.format);
    srca = src >> 24;
    if (src == 0)
	return;

    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, xuint32_t,
                           dst_stride, dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (mask_image, 0, mask_y, xuint32_t,
                           mask_stride, mask_line, 1);
    mask_line += mask_x >> 5;

    if (srca == 0xff)
    {
	while (height--)
	{
	    dst = dst_line;
	    dst_line += dst_stride;
	    mask = mask_line;
	    mask_line += mask_stride;
	    w = width;

	    bitcache = *mask++;
	    bitmask = CREATE_BITMASK (mask_x & 31);

	    while (w--)
	    {
		if (bitmask == 0)
		{
		    bitcache = *mask++;
		    bitmask = CREATE_BITMASK (0);
		}
		if (bitcache & bitmask)
		    *dst = src;
		bitmask = UPDATE_BITMASK (bitmask);
		dst++;
	    }
	}
    }
    else
    {
	while (height--)
	{
	    dst = dst_line;
	    dst_line += dst_stride;
	    mask = mask_line;
	    mask_line += mask_stride;
	    w = width;

	    bitcache = *mask++;
	    bitmask = CREATE_BITMASK (mask_x & 31);

	    while (w--)
	    {
		if (bitmask == 0)
		{
		    bitcache = *mask++;
		    bitmask = CREATE_BITMASK (0);
		}
		if (bitcache & bitmask)
		    *dst = over (src, *dst);
		bitmask = UPDATE_BITMASK (bitmask);
		dst++;
	    }
	}
    }
}

static void
fast_composite_over_n_1_0565 (pixman_implementation_t *imp,
                              pixman_composite_info_t *info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    xuint32_t     src, srca;
    xuint16_t    *dst, *dst_line;
    xuint32_t    *mask, *mask_line;
    int          mask_stride, dst_stride;
    xuint32_t     bitcache, bitmask;
    xint32_t      w;
    xuint32_t     d;
    xuint16_t     src565;

    if (width <= 0)
	return;

    src = _pixman_image_get_solid (imp, src_image, dest_image->bits.format);
    srca = src >> 24;
    if (src == 0)
	return;

    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, xuint16_t,
                           dst_stride, dst_line, 1);
    PIXMAN_IMAGE_GET_LINE (mask_image, 0, mask_y, xuint32_t,
                           mask_stride, mask_line, 1);
    mask_line += mask_x >> 5;

    if (srca == 0xff)
    {
	src565 = convert_8888_to_0565 (src);
	while (height--)
	{
	    dst = dst_line;
	    dst_line += dst_stride;
	    mask = mask_line;
	    mask_line += mask_stride;
	    w = width;

	    bitcache = *mask++;
	    bitmask = CREATE_BITMASK (mask_x & 31);

	    while (w--)
	    {
		if (bitmask == 0)
		{
		    bitcache = *mask++;
		    bitmask = CREATE_BITMASK (0);
		}
		if (bitcache & bitmask)
		    *dst = src565;
		bitmask = UPDATE_BITMASK (bitmask);
		dst++;
	    }
	}
    }
    else
    {
	while (height--)
	{
	    dst = dst_line;
	    dst_line += dst_stride;
	    mask = mask_line;
	    mask_line += mask_stride;
	    w = width;

	    bitcache = *mask++;
	    bitmask = CREATE_BITMASK (mask_x & 31);

	    while (w--)
	    {
		if (bitmask == 0)
		{
		    bitcache = *mask++;
		    bitmask = CREATE_BITMASK (0);
		}
		if (bitcache & bitmask)
		{
		    d = over (src, convert_0565_to_0888 (*dst));
		    *dst = convert_8888_to_0565 (d);
		}
		bitmask = UPDATE_BITMASK (bitmask);
		dst++;
	    }
	}
    }
}

/*
 * Simple bitblt
 */

static void
fast_composite_solid_fill (pixman_implementation_t *imp,
                           pixman_composite_info_t *info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    xuint32_t src;

    src = _pixman_image_get_solid (imp, src_image, dest_image->bits.format);

    if (dest_image->bits.format == PIXMAN_a1)
    {
	src = src >> 31;
    }
    else if (dest_image->bits.format == PIXMAN_a8)
    {
	src = src >> 24;
    }
    else if (dest_image->bits.format == PIXMAN_r5g6b5 ||
             dest_image->bits.format == PIXMAN_b5g6r5)
    {
	src = convert_8888_to_0565 (src);
    }

    pixman_fill (dest_image->bits.bits, dest_image->bits.rowstride,
                 PIXMAN_FORMAT_BPP (dest_image->bits.format),
                 dest_x, dest_y,
                 width, height,
                 src);
}

static void
fast_composite_src_memcpy (pixman_implementation_t *imp,
			   pixman_composite_info_t *info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    int bpp = PIXMAN_FORMAT_BPP (dest_image->bits.format) / 8;
    xuint32_t n_bytes = width * bpp;
    int dst_stride, src_stride;
    xuint8_t    *dst;
    xuint8_t    *src;

    src_stride = src_image->bits.rowstride * 4;
    dst_stride = dest_image->bits.rowstride * 4;

    src = (xuint8_t *)src_image->bits.bits + src_y * src_stride + src_x * bpp;
    dst = (xuint8_t *)dest_image->bits.bits + dest_y * dst_stride + dest_x * bpp;

    while (height--)
    {
    xmemory_copy (dst, src, n_bytes);

	dst += dst_stride;
	src += src_stride;
    }
}

FAST_NEAREST (8888_8888_cover, 8888, 8888, xuint32_t, xuint32_t, SRC, COVER)
FAST_NEAREST (8888_8888_none, 8888, 8888, xuint32_t, xuint32_t, SRC, NONE)
FAST_NEAREST (8888_8888_pad, 8888, 8888, xuint32_t, xuint32_t, SRC, PAD)
FAST_NEAREST (8888_8888_normal, 8888, 8888, xuint32_t, xuint32_t, SRC, NORMAL)
FAST_NEAREST (x888_8888_cover, x888, 8888, xuint32_t, xuint32_t, SRC, COVER)
FAST_NEAREST (x888_8888_pad, x888, 8888, xuint32_t, xuint32_t, SRC, PAD)
FAST_NEAREST (x888_8888_normal, x888, 8888, xuint32_t, xuint32_t, SRC, NORMAL)
FAST_NEAREST (8888_8888_cover, 8888, 8888, xuint32_t, xuint32_t, OVER, COVER)
FAST_NEAREST (8888_8888_none, 8888, 8888, xuint32_t, xuint32_t, OVER, NONE)
FAST_NEAREST (8888_8888_pad, 8888, 8888, xuint32_t, xuint32_t, OVER, PAD)
FAST_NEAREST (8888_8888_normal, 8888, 8888, xuint32_t, xuint32_t, OVER, NORMAL)
FAST_NEAREST (8888_565_cover, 8888, 0565, xuint32_t, xuint16_t, SRC, COVER)
FAST_NEAREST (8888_565_none, 8888, 0565, xuint32_t, xuint16_t, SRC, NONE)
FAST_NEAREST (8888_565_pad, 8888, 0565, xuint32_t, xuint16_t, SRC, PAD)
FAST_NEAREST (8888_565_normal, 8888, 0565, xuint32_t, xuint16_t, SRC, NORMAL)
FAST_NEAREST (565_565_normal, 0565, 0565, xuint16_t, xuint16_t, SRC, NORMAL)
FAST_NEAREST (8888_565_cover, 8888, 0565, xuint32_t, xuint16_t, OVER, COVER)
FAST_NEAREST (8888_565_none, 8888, 0565, xuint32_t, xuint16_t, OVER, NONE)
FAST_NEAREST (8888_565_pad, 8888, 0565, xuint32_t, xuint16_t, OVER, PAD)
FAST_NEAREST (8888_565_normal, 8888, 0565, xuint32_t, xuint16_t, OVER, NORMAL)

#define REPEAT_MIN_WIDTH    32

static void
fast_composite_tiled_repeat (pixman_implementation_t *imp,
			     pixman_composite_info_t *info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    pixman_composite_func_t func;
    pixman_format_code_t mask_format;
    xuint32_t src_flags, mask_flags;
    xint32_t sx, sy;
    xint32_t width_remain;
    xint32_t num_pixels;
    xint32_t src_width;
    xint32_t i, j;
    pixman_image_t extended_src_image;
    xuint32_t extended_src[REPEAT_MIN_WIDTH * 2];
    pixman_bool_t need_src_extension;
    xuint32_t *src_line;
    xint32_t src_stride;
    xint32_t src_bpp;
    pixman_composite_info_t info2 = *info;

    src_flags = (info->src_flags & ~FAST_PATH_NORMAL_REPEAT) |
		    FAST_PATH_SAMPLES_COVER_CLIP_NEAREST;

    if (mask_image)
    {
	mask_format = mask_image->common.extended_format_code;
	mask_flags = info->mask_flags;
    }
    else
    {
	mask_format = PIXMAN_null;
	mask_flags = FAST_PATH_IS_OPAQUE;
    }

    _pixman_implementation_lookup_composite (
	imp->toplevel, info->op,
	src_image->common.extended_format_code, src_flags,
	mask_format, mask_flags,
	dest_image->common.extended_format_code, info->dest_flags,
	&imp, &func);

    src_bpp = PIXMAN_FORMAT_BPP (src_image->bits.format);

    if (src_image->bits.width < REPEAT_MIN_WIDTH		&&
	(src_bpp == 32 || src_bpp == 16 || src_bpp == 8)	&&
	!src_image->bits.indexed)
    {
	sx = src_x;
	sx = MOD (sx, src_image->bits.width);
	sx += width;
	src_width = 0;

	while (src_width < REPEAT_MIN_WIDTH && src_width <= sx)
	    src_width += src_image->bits.width;

	src_stride = (src_width * (src_bpp >> 3) + 3) / (int) sizeof (xuint32_t);

	/* Initialize/validate stack-allocated temporary image */
	_pixman_bits_image_init (&extended_src_image, src_image->bits.format,
				 src_width, 1, &extended_src[0], src_stride,
				 FALSE);
	_pixman_image_validate (&extended_src_image);

	info2.src_image = &extended_src_image;
	need_src_extension = TRUE;
    }
    else
    {
	src_width = src_image->bits.width;
	need_src_extension = FALSE;
    }

    sx = src_x;
    sy = src_y;

    while (--height >= 0)
    {
	sx = MOD (sx, src_width);
	sy = MOD (sy, src_image->bits.height);

	if (need_src_extension)
	{
	    if (src_bpp == 32)
	    {
		PIXMAN_IMAGE_GET_LINE (src_image, 0, sy, xuint32_t, src_stride, src_line, 1);

		for (i = 0; i < src_width; )
		{
		    for (j = 0; j < src_image->bits.width; j++, i++)
			extended_src[i] = src_line[j];
		}
	    }
	    else if (src_bpp == 16)
	    {
		xuint16_t *src_line_16;

		PIXMAN_IMAGE_GET_LINE (src_image, 0, sy, xuint16_t, src_stride,
				       src_line_16, 1);
		src_line = (xuint32_t*)src_line_16;

		for (i = 0; i < src_width; )
		{
		    for (j = 0; j < src_image->bits.width; j++, i++)
			((xuint16_t*)extended_src)[i] = ((xuint16_t*)src_line)[j];
		}
	    }
	    else if (src_bpp == 8)
	    {
		xuint8_t *src_line_8;

		PIXMAN_IMAGE_GET_LINE (src_image, 0, sy, xuint8_t, src_stride,
				       src_line_8, 1);
		src_line = (xuint32_t*)src_line_8;

		for (i = 0; i < src_width; )
		{
		    for (j = 0; j < src_image->bits.width; j++, i++)
			((xuint8_t*)extended_src)[i] = ((xuint8_t*)src_line)[j];
		}
	    }

	    info2.src_y = 0;
	}
	else
	{
	    info2.src_y = sy;
	}

	width_remain = width;

	while (width_remain > 0)
	{
	    num_pixels = src_width - sx;

	    if (num_pixels > width_remain)
		num_pixels = width_remain;

	    info2.src_x = sx;
	    info2.width = num_pixels;
	    info2.height = 1;

	    func (imp, &info2);

	    width_remain -= num_pixels;
	    info2.mask_x += num_pixels;
	    info2.dest_x += num_pixels;
	    sx = 0;
	}

	sx = src_x;
	sy++;
	info2.mask_x = info->mask_x;
	info2.mask_y++;
	info2.dest_x = info->dest_x;
	info2.dest_y++;
    }

    if (need_src_extension)
	_pixman_image_fini (&extended_src_image);
}

/* Use more unrolling for src_0565_0565 because it is typically CPU bound */
static force_inline void
scaled_nearest_scanline_565_565_SRC (xuint16_t *       dst,
				     const xuint16_t * src,
				     xint32_t          w,
				     pixman_fixed_t   vx,
				     pixman_fixed_t   unit_x,
				     pixman_fixed_t   max_vx,
				     pixman_bool_t    fully_transparent_src)
{
    xuint16_t tmp1, tmp2, tmp3, tmp4;
    while ((w -= 4) >= 0)
    {
	tmp1 = *(src + pixman_fixed_to_int (vx));
	vx += unit_x;
	tmp2 = *(src + pixman_fixed_to_int (vx));
	vx += unit_x;
	tmp3 = *(src + pixman_fixed_to_int (vx));
	vx += unit_x;
	tmp4 = *(src + pixman_fixed_to_int (vx));
	vx += unit_x;
	*dst++ = tmp1;
	*dst++ = tmp2;
	*dst++ = tmp3;
	*dst++ = tmp4;
    }
    if (w & 2)
    {
	tmp1 = *(src + pixman_fixed_to_int (vx));
	vx += unit_x;
	tmp2 = *(src + pixman_fixed_to_int (vx));
	vx += unit_x;
	*dst++ = tmp1;
	*dst++ = tmp2;
    }
    if (w & 1)
	*dst = *(src + pixman_fixed_to_int (vx));
}

FAST_NEAREST_MAINLOOP (565_565_cover_SRC,
		       scaled_nearest_scanline_565_565_SRC,
		       xuint16_t, xuint16_t, COVER)
FAST_NEAREST_MAINLOOP (565_565_none_SRC,
		       scaled_nearest_scanline_565_565_SRC,
		       xuint16_t, xuint16_t, NONE)
FAST_NEAREST_MAINLOOP (565_565_pad_SRC,
		       scaled_nearest_scanline_565_565_SRC,
		       xuint16_t, xuint16_t, PAD)

static force_inline xuint32_t
fetch_nearest (pixman_repeat_t src_repeat,
	       pixman_format_code_t format,
	       xuint32_t *src, int x, int src_width)
{
    if (repeat (src_repeat, &x, src_width))
    {
	if (format == PIXMAN_x8r8g8b8 || format == PIXMAN_x8b8g8r8)
	    return *(src + x) | 0xff000000;
	else
	    return *(src + x);
    }
    else
    {
	return 0;
    }
}

static force_inline void
combine_over (xuint32_t s, xuint32_t *dst)
{
    if (s)
    {
	xuint8_t ia = 0xff - (s >> 24);

	if (ia)
	    UN8x4_MUL_UN8_ADD_UN8x4 (*dst, ia, s);
	else
	    *dst = s;
    }
}

static force_inline void
combine_src (xuint32_t s, xuint32_t *dst)
{
    *dst = s;
}

static void
fast_composite_scaled_nearest (pixman_implementation_t *imp,
			       pixman_composite_info_t *info)
{
    PIXMAN_COMPOSITE_ARGS (info);
    xuint32_t       *dst_line;
    xuint32_t       *src_line;
    int             dst_stride, src_stride;
    int		    src_width, src_height;
    pixman_repeat_t src_repeat;
    pixman_fixed_t unit_x, unit_y;
    pixman_format_code_t src_format;
    pixman_vector_t v;
    pixman_fixed_t vy;

    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, xuint32_t, dst_stride, dst_line, 1);
    /* pass in 0 instead of src_x and src_y because src_x and src_y need to be
     * transformed from destination space to source space
     */
    PIXMAN_IMAGE_GET_LINE (src_image, 0, 0, xuint32_t, src_stride, src_line, 1);

    /* reference point is the center of the pixel */
    v.vector[0] = pixman_int_to_fixed (src_x) + pixman_fixed_1 / 2;
    v.vector[1] = pixman_int_to_fixed (src_y) + pixman_fixed_1 / 2;
    v.vector[2] = pixman_fixed_1;

    if (!pixman_transform_point_3d (src_image->common.transform, &v))
	return;

    unit_x = src_image->common.transform->matrix[0][0];
    unit_y = src_image->common.transform->matrix[1][1];

    /* Round down to closest integer, ensuring that 0.5 rounds to 0, not 1 */
    v.vector[0] -= pixman_fixed_e;
    v.vector[1] -= pixman_fixed_e;

    src_height = src_image->bits.height;
    src_width = src_image->bits.width;
    src_repeat = src_image->common.repeat;
    src_format = src_image->bits.format;

    vy = v.vector[1];
    while (height--)
    {
        pixman_fixed_t vx = v.vector[0];
	int y = pixman_fixed_to_int (vy);
	xuint32_t *dst = dst_line;

	dst_line += dst_stride;

        /* adjust the y location by a unit vector in the y direction
         * this is equivalent to transforming y+1 of the destination point to source space */
        vy += unit_y;

	if (!repeat (src_repeat, &y, src_height))
	{
	    if (op == PIXMAN_OP_SRC)
        xmemory_set (dst, 0, sizeof (*dst) * width);
	}
	else
	{
	    int w = width;

	    xuint32_t *src = src_line + y * src_stride;

	    while (w >= 2)
	    {
		xuint32_t s1, s2;
		int x1, x2;

		x1 = pixman_fixed_to_int (vx);
		vx += unit_x;

		x2 = pixman_fixed_to_int (vx);
		vx += unit_x;

		w -= 2;

		s1 = fetch_nearest (src_repeat, src_format, src, x1, src_width);
		s2 = fetch_nearest (src_repeat, src_format, src, x2, src_width);

		if (op == PIXMAN_OP_OVER)
		{
		    combine_over (s1, dst++);
		    combine_over (s2, dst++);
		}
		else
		{
		    combine_src (s1, dst++);
		    combine_src (s2, dst++);
		}
	    }

	    while (w--)
	    {
		xuint32_t s;
		int x;

		x = pixman_fixed_to_int (vx);
		vx += unit_x;

		s = fetch_nearest (src_repeat, src_format, src, x, src_width);

		if (op == PIXMAN_OP_OVER)
		    combine_over (s, dst++);
		else
		    combine_src (s, dst++);
	    }
	}
    }
}

#define CACHE_LINE_SIZE 64

#define FAST_SIMPLE_ROTATE(suffix, pix_type)                                  \
                                                                              \
static void                                                                   \
blt_rotated_90_trivial_##suffix (pix_type       *dst,                         \
				 int             dst_stride,                  \
				 const pix_type *src,                         \
				 int             src_stride,                  \
				 int             w,                           \
				 int             h)                           \
{                                                                             \
    int x, y;                                                                 \
    for (y = 0; y < h; y++)                                                   \
    {                                                                         \
	const pix_type *s = src + (h - y - 1);                                \
	pix_type *d = dst + dst_stride * y;                                   \
	for (x = 0; x < w; x++)                                               \
	{                                                                     \
	    *d++ = *s;                                                        \
	    s += src_stride;                                                  \
	}                                                                     \
    }                                                                         \
}                                                                             \
                                                                              \
static void                                                                   \
blt_rotated_270_trivial_##suffix (pix_type       *dst,                        \
				  int             dst_stride,                 \
				  const pix_type *src,                        \
				  int             src_stride,                 \
				  int             w,                          \
				  int             h)                          \
{                                                                             \
    int x, y;                                                                 \
    for (y = 0; y < h; y++)                                                   \
    {                                                                         \
	const pix_type *s = src + src_stride * (w - 1) + y;                   \
	pix_type *d = dst + dst_stride * y;                                   \
	for (x = 0; x < w; x++)                                               \
	{                                                                     \
	    *d++ = *s;                                                        \
	    s -= src_stride;                                                  \
	}                                                                     \
    }                                                                         \
}                                                                             \
                                                                              \
static void                                                                   \
blt_rotated_90_##suffix (pix_type       *dst,                                 \
			 int             dst_stride,                          \
			 const pix_type *src,                                 \
			 int             src_stride,                          \
			 int             W,                                   \
			 int             H)                                   \
{                                                                             \
    int x;                                                                    \
    int leading_pixels = 0, trailing_pixels = 0;                              \
    const int TILE_SIZE = CACHE_LINE_SIZE / sizeof(pix_type);                 \
                                                                              \
    /*                                                                        \
     * split processing into handling destination as TILE_SIZExH cache line   \
     * aligned vertical stripes (optimistically assuming that destination     \
     * stride is a multiple of cache line, if not - it will be just a bit     \
     * slower)                                                                \
     */                                                                       \
                                                                              \
    if ((xuintptr_t)dst & (CACHE_LINE_SIZE - 1))                               \
    {                                                                         \
    leading_pixels = TILE_SIZE - (((xuintptr_t)dst &                       \
			    (CACHE_LINE_SIZE - 1)) / sizeof(pix_type));       \
	if (leading_pixels > W)                                               \
	    leading_pixels = W;                                               \
                                                                              \
	/* unaligned leading part NxH (where N < TILE_SIZE) */                \
	blt_rotated_90_trivial_##suffix (                                     \
	    dst,                                                              \
	    dst_stride,                                                       \
	    src,                                                              \
	    src_stride,                                                       \
	    leading_pixels,                                                   \
	    H);                                                               \
	                                                                      \
	dst += leading_pixels;                                                \
	src += leading_pixels * src_stride;                                   \
	W -= leading_pixels;                                                  \
    }                                                                         \
                                                                              \
    if ((xuintptr_t)(dst + W) & (CACHE_LINE_SIZE - 1))                         \
    {                                                                         \
    trailing_pixels = (((xuintptr_t)(dst + W) &                            \
			    (CACHE_LINE_SIZE - 1)) / sizeof(pix_type));       \
	if (trailing_pixels > W)                                              \
	    trailing_pixels = W;                                              \
	W -= trailing_pixels;                                                 \
    }                                                                         \
                                                                              \
    for (x = 0; x < W; x += TILE_SIZE)                                        \
    {                                                                         \
	/* aligned middle part TILE_SIZExH */                                 \
	blt_rotated_90_trivial_##suffix (                                     \
	    dst + x,                                                          \
	    dst_stride,                                                       \
	    src + src_stride * x,                                             \
	    src_stride,                                                       \
	    TILE_SIZE,                                                        \
	    H);                                                               \
    }                                                                         \
                                                                              \
    if (trailing_pixels)                                                      \
    {                                                                         \
	/* unaligned trailing part NxH (where N < TILE_SIZE) */               \
	blt_rotated_90_trivial_##suffix (                                     \
	    dst + W,                                                          \
	    dst_stride,                                                       \
	    src + W * src_stride,                                             \
	    src_stride,                                                       \
	    trailing_pixels,                                                  \
	    H);                                                               \
    }                                                                         \
}                                                                             \
                                                                              \
static void                                                                   \
blt_rotated_270_##suffix (pix_type       *dst,                                \
			  int             dst_stride,                         \
			  const pix_type *src,                                \
			  int             src_stride,                         \
			  int             W,                                  \
			  int             H)                                  \
{                                                                             \
    int x;                                                                    \
    int leading_pixels = 0, trailing_pixels = 0;                              \
    const int TILE_SIZE = CACHE_LINE_SIZE / sizeof(pix_type);                 \
                                                                              \
    /*                                                                        \
     * split processing into handling destination as TILE_SIZExH cache line   \
     * aligned vertical stripes (optimistically assuming that destination     \
     * stride is a multiple of cache line, if not - it will be just a bit     \
     * slower)                                                                \
     */                                                                       \
                                                                              \
    if ((xuintptr_t)dst & (CACHE_LINE_SIZE - 1))                               \
    {                                                                         \
    leading_pixels = TILE_SIZE - (((xuintptr_t)dst &                       \
			    (CACHE_LINE_SIZE - 1)) / sizeof(pix_type));       \
	if (leading_pixels > W)                                               \
	    leading_pixels = W;                                               \
                                                                              \
	/* unaligned leading part NxH (where N < TILE_SIZE) */                \
	blt_rotated_270_trivial_##suffix (                                    \
	    dst,                                                              \
	    dst_stride,                                                       \
	    src + src_stride * (W - leading_pixels),                          \
	    src_stride,                                                       \
	    leading_pixels,                                                   \
	    H);                                                               \
	                                                                      \
	dst += leading_pixels;                                                \
	W -= leading_pixels;                                                  \
    }                                                                         \
                                                                              \
    if ((xuintptr_t)(dst + W) & (CACHE_LINE_SIZE - 1))                         \
    {                                                                         \
    trailing_pixels = (((xuintptr_t)(dst + W) &                            \
			    (CACHE_LINE_SIZE - 1)) / sizeof(pix_type));       \
	if (trailing_pixels > W)                                              \
	    trailing_pixels = W;                                              \
	W -= trailing_pixels;                                                 \
	src += trailing_pixels * src_stride;                                  \
    }                                                                         \
                                                                              \
    for (x = 0; x < W; x += TILE_SIZE)                                        \
    {                                                                         \
	/* aligned middle part TILE_SIZExH */                                 \
	blt_rotated_270_trivial_##suffix (                                    \
	    dst + x,                                                          \
	    dst_stride,                                                       \
	    src + src_stride * (W - x - TILE_SIZE),                           \
	    src_stride,                                                       \
	    TILE_SIZE,                                                        \
	    H);                                                               \
    }                                                                         \
                                                                              \
    if (trailing_pixels)                                                      \
    {                                                                         \
	/* unaligned trailing part NxH (where N < TILE_SIZE) */               \
	blt_rotated_270_trivial_##suffix (                                    \
	    dst + W,                                                          \
	    dst_stride,                                                       \
	    src - trailing_pixels * src_stride,                               \
	    src_stride,                                                       \
	    trailing_pixels,                                                  \
	    H);                                                               \
    }                                                                         \
}                                                                             \
                                                                              \
static void                                                                   \
fast_composite_rotate_90_##suffix (pixman_implementation_t *imp,              \
				   pixman_composite_info_t *info)	      \
{									      \
    PIXMAN_COMPOSITE_ARGS (info);					      \
    pix_type       *dst_line;						      \
    pix_type       *src_line;                                                 \
    int             dst_stride, src_stride;                                   \
    int             src_x_t, src_y_t;                                         \
                                                                              \
    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, pix_type,              \
			   dst_stride, dst_line, 1);                          \
    src_x_t = -src_y + pixman_fixed_to_int (                                  \
				src_image->common.transform->matrix[0][2] +   \
				pixman_fixed_1 / 2 - pixman_fixed_e) - height;\
    src_y_t = src_x + pixman_fixed_to_int (                                   \
				src_image->common.transform->matrix[1][2] +   \
				pixman_fixed_1 / 2 - pixman_fixed_e);         \
    PIXMAN_IMAGE_GET_LINE (src_image, src_x_t, src_y_t, pix_type,             \
			   src_stride, src_line, 1);                          \
    blt_rotated_90_##suffix (dst_line, dst_stride, src_line, src_stride,      \
			     width, height);                                  \
}                                                                             \
                                                                              \
static void                                                                   \
fast_composite_rotate_270_##suffix (pixman_implementation_t *imp,             \
				    pixman_composite_info_t *info)            \
{                                                                             \
    PIXMAN_COMPOSITE_ARGS (info);					      \
    pix_type       *dst_line;						      \
    pix_type       *src_line;                                                 \
    int             dst_stride, src_stride;                                   \
    int             src_x_t, src_y_t;                                         \
                                                                              \
    PIXMAN_IMAGE_GET_LINE (dest_image, dest_x, dest_y, pix_type,              \
			   dst_stride, dst_line, 1);                          \
    src_x_t = src_y + pixman_fixed_to_int (                                   \
				src_image->common.transform->matrix[0][2] +   \
				pixman_fixed_1 / 2 - pixman_fixed_e);         \
    src_y_t = -src_x + pixman_fixed_to_int (                                  \
				src_image->common.transform->matrix[1][2] +   \
				pixman_fixed_1 / 2 - pixman_fixed_e) - width; \
    PIXMAN_IMAGE_GET_LINE (src_image, src_x_t, src_y_t, pix_type,             \
			   src_stride, src_line, 1);                          \
    blt_rotated_270_##suffix (dst_line, dst_stride, src_line, src_stride,     \
			      width, height);                                 \
}

FAST_SIMPLE_ROTATE (8, xuint8_t)
FAST_SIMPLE_ROTATE (565, xuint16_t)
FAST_SIMPLE_ROTATE (8888, xuint32_t)

static const pixman_fast_path_t c_fast_paths[] =
{
    PIXMAN_STD_FAST_PATH (OVER, solid, a8, r5g6b5, fast_composite_over_n_8_0565),
    PIXMAN_STD_FAST_PATH (OVER, solid, a8, b5g6r5, fast_composite_over_n_8_0565),
    PIXMAN_STD_FAST_PATH (OVER, solid, a8, r8g8b8, fast_composite_over_n_8_0888),
    PIXMAN_STD_FAST_PATH (OVER, solid, a8, b8g8r8, fast_composite_over_n_8_0888),
    PIXMAN_STD_FAST_PATH (OVER, solid, a8, a8r8g8b8, fast_composite_over_n_8_8888),
    PIXMAN_STD_FAST_PATH (OVER, solid, a8, x8r8g8b8, fast_composite_over_n_8_8888),
    PIXMAN_STD_FAST_PATH (OVER, solid, a8, a8b8g8r8, fast_composite_over_n_8_8888),
    PIXMAN_STD_FAST_PATH (OVER, solid, a8, x8b8g8r8, fast_composite_over_n_8_8888),
    PIXMAN_STD_FAST_PATH (OVER, solid, a1, a8r8g8b8, fast_composite_over_n_1_8888),
    PIXMAN_STD_FAST_PATH (OVER, solid, a1, x8r8g8b8, fast_composite_over_n_1_8888),
    PIXMAN_STD_FAST_PATH (OVER, solid, a1, a8b8g8r8, fast_composite_over_n_1_8888),
    PIXMAN_STD_FAST_PATH (OVER, solid, a1, x8b8g8r8, fast_composite_over_n_1_8888),
    PIXMAN_STD_FAST_PATH (OVER, solid, a1, r5g6b5,   fast_composite_over_n_1_0565),
    PIXMAN_STD_FAST_PATH (OVER, solid, a1, b5g6r5,   fast_composite_over_n_1_0565),
    PIXMAN_STD_FAST_PATH_CA (OVER, solid, a8r8g8b8, a8r8g8b8, fast_composite_over_n_8888_8888_ca),
    PIXMAN_STD_FAST_PATH_CA (OVER, solid, a8r8g8b8, x8r8g8b8, fast_composite_over_n_8888_8888_ca),
    PIXMAN_STD_FAST_PATH_CA (OVER, solid, a8r8g8b8, r5g6b5, fast_composite_over_n_8888_0565_ca),
    PIXMAN_STD_FAST_PATH_CA (OVER, solid, a8b8g8r8, a8b8g8r8, fast_composite_over_n_8888_8888_ca),
    PIXMAN_STD_FAST_PATH_CA (OVER, solid, a8b8g8r8, x8b8g8r8, fast_composite_over_n_8888_8888_ca),
    PIXMAN_STD_FAST_PATH_CA (OVER, solid, a8b8g8r8, b5g6r5, fast_composite_over_n_8888_0565_ca),
    PIXMAN_STD_FAST_PATH (OVER, x8r8g8b8, a8, x8r8g8b8, fast_composite_over_x888_8_8888),
    PIXMAN_STD_FAST_PATH (OVER, x8r8g8b8, a8, a8r8g8b8, fast_composite_over_x888_8_8888),
    PIXMAN_STD_FAST_PATH (OVER, x8b8g8r8, a8, x8b8g8r8, fast_composite_over_x888_8_8888),
    PIXMAN_STD_FAST_PATH (OVER, x8b8g8r8, a8, a8b8g8r8, fast_composite_over_x888_8_8888),
    PIXMAN_STD_FAST_PATH (OVER, a8r8g8b8, null, a8r8g8b8, fast_composite_over_8888_8888),
    PIXMAN_STD_FAST_PATH (OVER, a8r8g8b8, null, x8r8g8b8, fast_composite_over_8888_8888),
    PIXMAN_STD_FAST_PATH (OVER, a8r8g8b8, null, r5g6b5, fast_composite_over_8888_0565),
    PIXMAN_STD_FAST_PATH (OVER, a8b8g8r8, null, a8b8g8r8, fast_composite_over_8888_8888),
    PIXMAN_STD_FAST_PATH (OVER, a8b8g8r8, null, x8b8g8r8, fast_composite_over_8888_8888),
    PIXMAN_STD_FAST_PATH (OVER, a8b8g8r8, null, b5g6r5, fast_composite_over_8888_0565),
    PIXMAN_STD_FAST_PATH (ADD, r5g6b5, null, r5g6b5, fast_composite_add_0565_0565),
    PIXMAN_STD_FAST_PATH (ADD, b5g6r5, null, b5g6r5, fast_composite_add_0565_0565),
    PIXMAN_STD_FAST_PATH (ADD, a8r8g8b8, null, a8r8g8b8, fast_composite_add_8888_8888),
    PIXMAN_STD_FAST_PATH (ADD, a8b8g8r8, null, a8b8g8r8, fast_composite_add_8888_8888),
    PIXMAN_STD_FAST_PATH (ADD, a8, null, a8, fast_composite_add_8_8),
    PIXMAN_STD_FAST_PATH (ADD, a1, null, a1, fast_composite_add_1_1),
    PIXMAN_STD_FAST_PATH_CA (ADD, solid, a8r8g8b8, a8r8g8b8, fast_composite_add_n_8888_8888_ca),
    PIXMAN_STD_FAST_PATH (ADD, solid, a8, a8, fast_composite_add_n_8_8),
    PIXMAN_STD_FAST_PATH (SRC, solid, null, a8r8g8b8, fast_composite_solid_fill),
    PIXMAN_STD_FAST_PATH (SRC, solid, null, x8r8g8b8, fast_composite_solid_fill),
    PIXMAN_STD_FAST_PATH (SRC, solid, null, a8b8g8r8, fast_composite_solid_fill),
    PIXMAN_STD_FAST_PATH (SRC, solid, null, x8b8g8r8, fast_composite_solid_fill),
    PIXMAN_STD_FAST_PATH (SRC, solid, null, a1, fast_composite_solid_fill),
    PIXMAN_STD_FAST_PATH (SRC, solid, null, a8, fast_composite_solid_fill),
    PIXMAN_STD_FAST_PATH (SRC, solid, null, r5g6b5, fast_composite_solid_fill),
    PIXMAN_STD_FAST_PATH (SRC, x8r8g8b8, null, a8r8g8b8, fast_composite_src_x888_8888),
    PIXMAN_STD_FAST_PATH (SRC, x8b8g8r8, null, a8b8g8r8, fast_composite_src_x888_8888),
    PIXMAN_STD_FAST_PATH (SRC, a8r8g8b8, null, x8r8g8b8, fast_composite_src_memcpy),
    PIXMAN_STD_FAST_PATH (SRC, a8r8g8b8, null, a8r8g8b8, fast_composite_src_memcpy),
    PIXMAN_STD_FAST_PATH (SRC, x8r8g8b8, null, x8r8g8b8, fast_composite_src_memcpy),
    PIXMAN_STD_FAST_PATH (SRC, a8b8g8r8, null, x8b8g8r8, fast_composite_src_memcpy),
    PIXMAN_STD_FAST_PATH (SRC, a8b8g8r8, null, a8b8g8r8, fast_composite_src_memcpy),
    PIXMAN_STD_FAST_PATH (SRC, x8b8g8r8, null, x8b8g8r8, fast_composite_src_memcpy),
    PIXMAN_STD_FAST_PATH (SRC, b8g8r8a8, null, b8g8r8x8, fast_composite_src_memcpy),
    PIXMAN_STD_FAST_PATH (SRC, b8g8r8a8, null, b8g8r8a8, fast_composite_src_memcpy),
    PIXMAN_STD_FAST_PATH (SRC, b8g8r8x8, null, b8g8r8x8, fast_composite_src_memcpy),
    PIXMAN_STD_FAST_PATH (SRC, r5g6b5, null, r5g6b5, fast_composite_src_memcpy),
    PIXMAN_STD_FAST_PATH (SRC, b5g6r5, null, b5g6r5, fast_composite_src_memcpy),
    PIXMAN_STD_FAST_PATH (SRC, r8g8b8, null, r8g8b8, fast_composite_src_memcpy),
    PIXMAN_STD_FAST_PATH (SRC, b8g8r8, null, b8g8r8, fast_composite_src_memcpy),
    PIXMAN_STD_FAST_PATH (SRC, x1r5g5b5, null, x1r5g5b5, fast_composite_src_memcpy),
    PIXMAN_STD_FAST_PATH (SRC, a1r5g5b5, null, x1r5g5b5, fast_composite_src_memcpy),
    PIXMAN_STD_FAST_PATH (SRC, a8, null, a8, fast_composite_src_memcpy),
    PIXMAN_STD_FAST_PATH (IN, a8, null, a8, fast_composite_in_8_8),
    PIXMAN_STD_FAST_PATH (IN, solid, a8, a8, fast_composite_in_n_8_8),

    SIMPLE_NEAREST_FAST_PATH (SRC, x8r8g8b8, x8r8g8b8, 8888_8888),
    SIMPLE_NEAREST_FAST_PATH (SRC, a8r8g8b8, x8r8g8b8, 8888_8888),
    SIMPLE_NEAREST_FAST_PATH (SRC, x8b8g8r8, x8b8g8r8, 8888_8888),
    SIMPLE_NEAREST_FAST_PATH (SRC, a8b8g8r8, x8b8g8r8, 8888_8888),

    SIMPLE_NEAREST_FAST_PATH (SRC, a8r8g8b8, a8r8g8b8, 8888_8888),
    SIMPLE_NEAREST_FAST_PATH (SRC, a8b8g8r8, a8b8g8r8, 8888_8888),

    SIMPLE_NEAREST_FAST_PATH (SRC, x8r8g8b8, r5g6b5, 8888_565),
    SIMPLE_NEAREST_FAST_PATH (SRC, a8r8g8b8, r5g6b5, 8888_565),

    SIMPLE_NEAREST_FAST_PATH (SRC, r5g6b5, r5g6b5, 565_565),

    SIMPLE_NEAREST_FAST_PATH_COVER (SRC, x8r8g8b8, a8r8g8b8, x888_8888),
    SIMPLE_NEAREST_FAST_PATH_COVER (SRC, x8b8g8r8, a8b8g8r8, x888_8888),
    SIMPLE_NEAREST_FAST_PATH_PAD (SRC, x8r8g8b8, a8r8g8b8, x888_8888),
    SIMPLE_NEAREST_FAST_PATH_PAD (SRC, x8b8g8r8, a8b8g8r8, x888_8888),
    SIMPLE_NEAREST_FAST_PATH_NORMAL (SRC, x8r8g8b8, a8r8g8b8, x888_8888),
    SIMPLE_NEAREST_FAST_PATH_NORMAL (SRC, x8b8g8r8, a8b8g8r8, x888_8888),

    SIMPLE_NEAREST_FAST_PATH (OVER, a8r8g8b8, x8r8g8b8, 8888_8888),
    SIMPLE_NEAREST_FAST_PATH (OVER, a8b8g8r8, x8b8g8r8, 8888_8888),
    SIMPLE_NEAREST_FAST_PATH (OVER, a8r8g8b8, a8r8g8b8, 8888_8888),
    SIMPLE_NEAREST_FAST_PATH (OVER, a8b8g8r8, a8b8g8r8, 8888_8888),

    SIMPLE_NEAREST_FAST_PATH (OVER, a8r8g8b8, r5g6b5, 8888_565),

#define NEAREST_FAST_PATH(op,s,d)		\
    {   PIXMAN_OP_ ## op,			\
	PIXMAN_ ## s, SCALED_NEAREST_FLAGS,	\
	PIXMAN_null, 0,				\
	PIXMAN_ ## d, FAST_PATH_STD_DEST_FLAGS,	\
	fast_composite_scaled_nearest,		\
    }

    NEAREST_FAST_PATH (SRC, x8r8g8b8, x8r8g8b8),
    NEAREST_FAST_PATH (SRC, a8r8g8b8, x8r8g8b8),
    NEAREST_FAST_PATH (SRC, x8b8g8r8, x8b8g8r8),
    NEAREST_FAST_PATH (SRC, a8b8g8r8, x8b8g8r8),

    NEAREST_FAST_PATH (SRC, x8r8g8b8, a8r8g8b8),
    NEAREST_FAST_PATH (SRC, a8r8g8b8, a8r8g8b8),
    NEAREST_FAST_PATH (SRC, x8b8g8r8, a8b8g8r8),
    NEAREST_FAST_PATH (SRC, a8b8g8r8, a8b8g8r8),

    NEAREST_FAST_PATH (OVER, x8r8g8b8, x8r8g8b8),
    NEAREST_FAST_PATH (OVER, a8r8g8b8, x8r8g8b8),
    NEAREST_FAST_PATH (OVER, x8b8g8r8, x8b8g8r8),
    NEAREST_FAST_PATH (OVER, a8b8g8r8, x8b8g8r8),

    NEAREST_FAST_PATH (OVER, x8r8g8b8, a8r8g8b8),
    NEAREST_FAST_PATH (OVER, a8r8g8b8, a8r8g8b8),
    NEAREST_FAST_PATH (OVER, x8b8g8r8, a8b8g8r8),
    NEAREST_FAST_PATH (OVER, a8b8g8r8, a8b8g8r8),

#define SIMPLE_ROTATE_FLAGS(angle)					  \
    (FAST_PATH_ROTATE_ ## angle ## _TRANSFORM	|			  \
     FAST_PATH_NEAREST_FILTER			|			  \
     FAST_PATH_SAMPLES_COVER_CLIP_NEAREST	|			  \
     FAST_PATH_STANDARD_FLAGS)

#define SIMPLE_ROTATE_FAST_PATH(op,s,d,suffix)				  \
    {   PIXMAN_OP_ ## op,						  \
	PIXMAN_ ## s, SIMPLE_ROTATE_FLAGS (90),				  \
	PIXMAN_null, 0,							  \
	PIXMAN_ ## d, FAST_PATH_STD_DEST_FLAGS,				  \
	fast_composite_rotate_90_##suffix,				  \
    },									  \
    {   PIXMAN_OP_ ## op,						  \
	PIXMAN_ ## s, SIMPLE_ROTATE_FLAGS (270),			  \
	PIXMAN_null, 0,							  \
	PIXMAN_ ## d, FAST_PATH_STD_DEST_FLAGS,				  \
	fast_composite_rotate_270_##suffix,				  \
    }

    SIMPLE_ROTATE_FAST_PATH (SRC, a8r8g8b8, a8r8g8b8, 8888),
    SIMPLE_ROTATE_FAST_PATH (SRC, a8r8g8b8, x8r8g8b8, 8888),
    SIMPLE_ROTATE_FAST_PATH (SRC, x8r8g8b8, x8r8g8b8, 8888),
    SIMPLE_ROTATE_FAST_PATH (SRC, r5g6b5, r5g6b5, 565),
    SIMPLE_ROTATE_FAST_PATH (SRC, a8, a8, 8),

    /* Simple repeat fast path entry. */
    {	PIXMAN_OP_any,
	PIXMAN_any,
	(FAST_PATH_STANDARD_FLAGS | FAST_PATH_ID_TRANSFORM | FAST_PATH_BITS_IMAGE |
	 FAST_PATH_NORMAL_REPEAT),
	PIXMAN_any, 0,
	PIXMAN_any, FAST_PATH_STD_DEST_FLAGS,
	fast_composite_tiled_repeat
    },

    {   PIXMAN_OP_NONE	},
};

#ifdef WORDS_BIGENDIAN
#define A1_FILL_MASK(n, offs) (((1U << (n)) - 1) << (32 - (offs) - (n)))
#else
#define A1_FILL_MASK(n, offs) (((1U << (n)) - 1) << (offs))
#endif

static force_inline void
pixman_fill1_line (xuint32_t *dst, int offs, int width, int v)
{
    if (offs)
    {
	int leading_pixels = 32 - offs;
	if (leading_pixels >= width)
	{
	    if (v)
		*dst |= A1_FILL_MASK (width, offs);
	    else
		*dst &= ~A1_FILL_MASK (width, offs);
	    return;
	}
	else
	{
	    if (v)
		*dst++ |= A1_FILL_MASK (leading_pixels, offs);
	    else
		*dst++ &= ~A1_FILL_MASK (leading_pixels, offs);
	    width -= leading_pixels;
	}
    }
    while (width >= 32)
    {
	if (v)
	    *dst++ = 0xFFFFFFFF;
	else
	    *dst++ = 0;
	width -= 32;
    }
    if (width > 0)
    {
	if (v)
	    *dst |= A1_FILL_MASK (width, 0);
	else
	    *dst &= ~A1_FILL_MASK (width, 0);
    }
}

static void
pixman_fill1 (xuint32_t *bits,
              int       stride,
              int       x,
              int       y,
              int       width,
              int       height,
              xuint32_t  filler)
{
    xuint32_t *dst = bits + y * stride + (x >> 5);
    int offs = x & 31;

    if (filler & 1)
    {
	while (height--)
	{
	    pixman_fill1_line (dst, offs, width, 1);
	    dst += stride;
	}
    }
    else
    {
	while (height--)
	{
	    pixman_fill1_line (dst, offs, width, 0);
	    dst += stride;
	}
    }
}

static void
pixman_fill8 (xuint32_t *bits,
              int       stride,
              int       x,
              int       y,
              int       width,
              int       height,
              xuint32_t  filler)
{
    int byte_stride = stride * (int) sizeof (xuint32_t);
    xuint8_t *dst = (xuint8_t *) bits;
    xuint8_t v = filler & 0xff;
    int i;

    dst = dst + y * byte_stride + x;

    while (height--)
    {
	for (i = 0; i < width; ++i)
	    dst[i] = v;

	dst += byte_stride;
    }
}

static void
pixman_fill16 (xuint32_t *bits,
               int       stride,
               int       x,
               int       y,
               int       width,
               int       height,
               xuint32_t  filler)
{
    int short_stride =
	(stride * (int)sizeof (xuint32_t)) / (int)sizeof (xuint16_t);
    xuint16_t *dst = (xuint16_t *)bits;
    xuint16_t v = filler & 0xffff;
    int i;

    dst = dst + y * short_stride + x;

    while (height--)
    {
	for (i = 0; i < width; ++i)
	    dst[i] = v;

	dst += short_stride;
    }
}

static void
pixman_fill32 (xuint32_t *bits,
               int       stride,
               int       x,
               int       y,
               int       width,
               int       height,
               xuint32_t  filler)
{
    int i;

    bits = bits + y * stride + x;

    while (height--)
    {
	for (i = 0; i < width; ++i)
	    bits[i] = filler;

	bits += stride;
    }
}

static pixman_bool_t
fast_path_fill (pixman_implementation_t *imp,
                xuint32_t *               bits,
                int                      stride,
                int                      bpp,
                int                      x,
                int                      y,
                int                      width,
                int                      height,
                xuint32_t		 filler)
{
    switch (bpp)
    {
    case 1:
	pixman_fill1 (bits, stride, x, y, width, height, filler);
	break;

    case 8:
	pixman_fill8 (bits, stride, x, y, width, height, filler);
	break;

    case 16:
	pixman_fill16 (bits, stride, x, y, width, height, filler);
	break;

    case 32:
	pixman_fill32 (bits, stride, x, y, width, height, filler);
	break;

    default:
	return FALSE;
    }

    return TRUE;
}

/*****************************************************************************/

static xuint32_t *
fast_fetch_r5g6b5 (pixman_iter_t *iter, const xuint32_t *mask)
{
    xint32_t w = iter->width;
    xuint32_t *dst = iter->buffer;
    const xuint16_t *src = (const xuint16_t *)iter->bits;

    iter->bits += iter->stride;

    /* Align the source buffer at 4 bytes boundary */
    if (w > 0 && ((xuintptr_t)src & 3))
    {
	*dst++ = convert_0565_to_8888 (*src++);
	w--;
    }
    /* Process two pixels per iteration */
    while ((w -= 2) >= 0)
    {
	xuint32_t sr, sb, sg, t0, t1;
	xuint32_t s = *(const xuint32_t *)src;
	src += 2;
	sr = (s >> 8) & 0x00F800F8;
	sb = (s << 3) & 0x00F800F8;
	sg = (s >> 3) & 0x00FC00FC;
	sr |= sr >> 5;
	sb |= sb >> 5;
	sg |= sg >> 6;
	t0 = ((sr << 16) & 0x00FF0000) | ((sg << 8) & 0x0000FF00) |
	     (sb & 0xFF) | 0xFF000000;
	t1 = (sr & 0x00FF0000) | ((sg >> 8) & 0x0000FF00) |
	     (sb >> 16) | 0xFF000000;
#ifdef WORDS_BIGENDIAN
	*dst++ = t1;
	*dst++ = t0;
#else
	*dst++ = t0;
	*dst++ = t1;
#endif
    }
    if (w & 1)
    {
	*dst = convert_0565_to_8888 (*src);
    }

    return iter->buffer;
}

static xuint32_t *
fast_dest_fetch_noop (pixman_iter_t *iter, const xuint32_t *mask)
{
    iter->bits += iter->stride;
    return iter->buffer;
}

/* Helper function for a workaround, which tries to ensure that 0x1F001F
 * constant is always allocated in a register on RISC architectures.
 */
static force_inline xuint32_t
convert_8888_to_0565_workaround (xuint32_t s, xuint32_t x1F001F)
{
    xuint32_t a, b;
    a = (s >> 3) & x1F001F;
    b = s & 0xFC00;
    a |= a >> 5;
    a |= b >> 5;
    return a;
}

static void
fast_write_back_r5g6b5 (pixman_iter_t *iter)
{
    xint32_t w = iter->width;
    xuint16_t *dst = (xuint16_t *)(iter->bits - iter->stride);
    const xuint32_t *src = iter->buffer;
    /* Workaround to ensure that x1F001F variable is allocated in a register */
    static volatile xuint32_t volatile_x1F001F = 0x1F001F;
    xuint32_t x1F001F = volatile_x1F001F;

    while ((w -= 4) >= 0)
    {
	xuint32_t s1 = *src++;
	xuint32_t s2 = *src++;
	xuint32_t s3 = *src++;
	xuint32_t s4 = *src++;
	*dst++ = convert_8888_to_0565_workaround (s1, x1F001F);
	*dst++ = convert_8888_to_0565_workaround (s2, x1F001F);
	*dst++ = convert_8888_to_0565_workaround (s3, x1F001F);
	*dst++ = convert_8888_to_0565_workaround (s4, x1F001F);
    }
    if (w & 2)
    {
	*dst++ = convert_8888_to_0565_workaround (*src++, x1F001F);
	*dst++ = convert_8888_to_0565_workaround (*src++, x1F001F);
    }
    if (w & 1)
    {
	*dst = convert_8888_to_0565_workaround (*src, x1F001F);
    }
}

typedef struct
{
    int		y;
    xuint64_t *	buffer;
} line_t;

typedef struct
{
    line_t		lines[2];
    pixman_fixed_t	y;
    pixman_fixed_t	x;
    xuint64_t		data[1];
} bilinear_info_t;

static void
fetch_horizontal (bits_image_t *image, line_t *line,
		  int y, pixman_fixed_t x, pixman_fixed_t ux, int n)
{
    xuint32_t *bits = image->bits + y * image->rowstride;
    int i;

    for (i = 0; i < n; ++i)
    {
	int x0 = pixman_fixed_to_int (x);
	int x1 = x0 + 1;
	xint32_t dist_x;

	xuint32_t left = *(bits + x0);
	xuint32_t right = *(bits + x1);

	dist_x = pixman_fixed_to_bilinear_weight (x);
	dist_x <<= (8 - BILINEAR_INTERPOLATION_BITS);

#if SIZEOF_LONG <= 4
	{
	    xuint32_t lag, rag, ag;
	    xuint32_t lrb, rrb, rb;

	    lag = (left & 0xff00ff00) >> 8;
	    rag = (right & 0xff00ff00) >> 8;
	    ag = (lag << 8) + dist_x * (rag - lag);

	    lrb = (left & 0x00ff00ff);
	    rrb = (right & 0x00ff00ff);
	    rb = (lrb << 8) + dist_x * (rrb - lrb);

	    *((xuint32_t *)(line->buffer + i)) = ag;
	    *((xuint32_t *)(line->buffer + i) + 1) = rb;
	}
#else
	{
	    xuint64_t lagrb, ragrb;
	    xuint32_t lag, rag;
	    xuint32_t lrb, rrb;

	    lag = (left & 0xff00ff00);
	    lrb = (left & 0x00ff00ff);
	    rag = (right & 0xff00ff00);
	    rrb = (right & 0x00ff00ff);
	    lagrb = (((xuint64_t)lag) << 24) | lrb;
	    ragrb = (((xuint64_t)rag) << 24) | rrb;

	    line->buffer[i] = (lagrb << 8) + dist_x * (ragrb - lagrb);
	}
#endif

	x += ux;
    }

    line->y = y;
}

static xuint32_t *
fast_fetch_bilinear_cover (pixman_iter_t *iter, const xuint32_t *mask)
{
    pixman_fixed_t fx, ux;
    bilinear_info_t *info = iter->data;
    line_t *line0, *line1;
    int y0, y1;
    xint32_t dist_y;
    int i;

    fx = info->x;
    ux = iter->image->common.transform->matrix[0][0];

    y0 = pixman_fixed_to_int (info->y);
    y1 = y0 + 1;
    dist_y = pixman_fixed_to_bilinear_weight (info->y);
    dist_y <<= (8 - BILINEAR_INTERPOLATION_BITS);

    line0 = &info->lines[y0 & 0x01];
    line1 = &info->lines[y1 & 0x01];

    if (line0->y != y0)
    {
	fetch_horizontal (
	    &iter->image->bits, line0, y0, fx, ux, iter->width);
    }

    if (line1->y != y1)
    {
	fetch_horizontal (
	    &iter->image->bits, line1, y1, fx, ux, iter->width);
    }

    for (i = 0; i < iter->width; ++i)
    {
#if SIZEOF_LONG <= 4
	xuint32_t ta, tr, tg, tb;
	xuint32_t ba, br, bg, bb;
	xuint32_t tag, trb;
	xuint32_t bag, brb;
	xuint32_t a, r, g, b;

	tag = *((xuint32_t *)(line0->buffer + i));
	trb = *((xuint32_t *)(line0->buffer + i) + 1);
	bag = *((xuint32_t *)(line1->buffer + i));
	brb = *((xuint32_t *)(line1->buffer + i) + 1);

	ta = tag >> 16;
	ba = bag >> 16;
	a = (ta << 8) + dist_y * (ba - ta);

	tr = trb >> 16;
	br = brb >> 16;
	r = (tr << 8) + dist_y * (br - tr);

	tg = tag & 0xffff;
	bg = bag & 0xffff;
	g = (tg << 8) + dist_y * (bg - tg);
	
	tb = trb & 0xffff;
	bb = brb & 0xffff;
	b = (tb << 8) + dist_y * (bb - tb);

	a = (a <<  8) & 0xff000000;
	r = (r <<  0) & 0x00ff0000;
	g = (g >>  8) & 0x0000ff00;
	b = (b >> 16) & 0x000000ff;
#else
	xuint64_t top = line0->buffer[i];
	xuint64_t bot = line1->buffer[i];
	xuint64_t tar = (top & 0xffff0000ffff0000ULL) >> 16;
	xuint64_t bar = (bot & 0xffff0000ffff0000ULL) >> 16;
	xuint64_t tgb = (top & 0x0000ffff0000ffffULL);
	xuint64_t bgb = (bot & 0x0000ffff0000ffffULL);
	xuint64_t ar, gb;
	xuint32_t a, r, g, b;

	ar = (tar << 8) + dist_y * (bar - tar);
	gb = (tgb << 8) + dist_y * (bgb - tgb);

	a = ((ar >> 24) & 0xff000000);
	r = ((ar >>  0) & 0x00ff0000);
	g = ((gb >> 40) & 0x0000ff00);
	b = ((gb >> 16) & 0x000000ff);
#endif

	iter->buffer[i] = a | r | g | b;
    }

    info->y += iter->image->common.transform->matrix[1][1];

    return iter->buffer;
}

static void
bilinear_cover_iter_fini (pixman_iter_t *iter)
{
    xmemory_free (iter->data);
}

static void
fast_bilinear_cover_iter_init (pixman_iter_t *iter, const pixman_iter_info_t *iter_info)
{
    int width = iter->width;
    bilinear_info_t *info;
    pixman_vector_t v;

    /* Reference point is the center of the pixel */
    v.vector[0] = pixman_int_to_fixed (iter->x) + pixman_fixed_1 / 2;
    v.vector[1] = pixman_int_to_fixed (iter->y) + pixman_fixed_1 / 2;
    v.vector[2] = pixman_fixed_1;

    if (!pixman_transform_point_3d (iter->image->common.transform, &v))
	goto fail;

    info = xmemory_alloc (sizeof (*info) + (2 * width - 1) * sizeof (xuint64_t));
    if (!info)
	goto fail;

    info->x = v.vector[0] - pixman_fixed_1 / 2;
    info->y = v.vector[1] - pixman_fixed_1 / 2;

    /* It is safe to set the y coordinates to -1 initially
     * because COVER_CLIP_BILINEAR ensures that we will only
     * be asked to fetch lines in the [0, height) interval
     */
    info->lines[0].y = -1;
    info->lines[0].buffer = &(info->data[0]);
    info->lines[1].y = -1;
    info->lines[1].buffer = &(info->data[width]);

    iter->get_scanline = fast_fetch_bilinear_cover;
    iter->fini = bilinear_cover_iter_fini;

    iter->data = info;
    return;

fail:
    /* Something went wrong, either a bad matrix or OOM; in such cases,
     * we don't guarantee any particular rendering.
     */
    _pixman_log_error (
	FUNC, "Allocation failure or bad matrix, skipping rendering\n");
    
    iter->get_scanline = _pixman_iter_get_scanline_noop;
    iter->fini = XNULL;
}

static xuint32_t *
bits_image_fetch_bilinear_no_repeat_8888 (pixman_iter_t *iter,
					  const xuint32_t *mask)
{

    pixman_image_t * ima = iter->image;
    int              offset = iter->x;
    int              line = iter->y++;
    int              width = iter->width;
    xuint32_t *       buffer = iter->buffer;

    bits_image_t *bits = &ima->bits;
    pixman_fixed_t x_top, x_bottom, x;
    pixman_fixed_t ux_top, ux_bottom, ux;
    pixman_vector_t v;
    xuint32_t top_mask, bottom_mask;
    xuint32_t *top_row;
    xuint32_t *bottom_row;
    xuint32_t *end;
    xuint32_t zero[2] = { 0, 0 };
    xuint32_t one = 1;
    int y, y1, y2;
    int disty;
    int mask_inc;
    int w;

    /* reference point is the center of the pixel */
    v.vector[0] = pixman_int_to_fixed (offset) + pixman_fixed_1 / 2;
    v.vector[1] = pixman_int_to_fixed (line) + pixman_fixed_1 / 2;
    v.vector[2] = pixman_fixed_1;

    if (!pixman_transform_point_3d (bits->common.transform, &v))
	return iter->buffer;

    ux = ux_top = ux_bottom = bits->common.transform->matrix[0][0];
    x = x_top = x_bottom = v.vector[0] - pixman_fixed_1/2;

    y = v.vector[1] - pixman_fixed_1/2;
    disty = pixman_fixed_to_bilinear_weight (y);

    /* Load the pointers to the first and second lines from the source
     * image that bilinear code must read.
     *
     * The main trick in this code is about the check if any line are
     * outside of the image;
     *
     * When I realize that a line (any one) is outside, I change
     * the pointer to a dummy area with zeros. Once I change this, I
     * must be sure the pointer will not change, so I set the
     * variables to each pointer increments inside the loop.
     */
    y1 = pixman_fixed_to_int (y);
    y2 = y1 + 1;

    if (y1 < 0 || y1 >= bits->height)
    {
	top_row = zero;
	x_top = 0;
	ux_top = 0;
    }
    else
    {
	top_row = bits->bits + y1 * bits->rowstride;
	x_top = x;
	ux_top = ux;
    }

    if (y2 < 0 || y2 >= bits->height)
    {
	bottom_row = zero;
	x_bottom = 0;
	ux_bottom = 0;
    }
    else
    {
	bottom_row = bits->bits + y2 * bits->rowstride;
	x_bottom = x;
	ux_bottom = ux;
    }

    /* Instead of checking whether the operation uses the mast in
     * each loop iteration, verify this only once and prepare the
     * variables to make the code smaller inside the loop.
     */
    if (!mask)
    {
        mask_inc = 0;
        mask = &one;
    }
    else
    {
        /* If have a mask, prepare the variables to check it */
        mask_inc = 1;
    }

    /* If both are zero, then the whole thing is zero */
    if (top_row == zero && bottom_row == zero)
    {
    xmemory_set (buffer, 0, width * sizeof (xuint32_t));
	return iter->buffer;
    }
    else if (bits->format == PIXMAN_x8r8g8b8)
    {
	if (top_row == zero)
	{
	    top_mask = 0;
	    bottom_mask = 0xff000000;
	}
	else if (bottom_row == zero)
	{
	    top_mask = 0xff000000;
	    bottom_mask = 0;
	}
	else
	{
	    top_mask = 0xff000000;
	    bottom_mask = 0xff000000;
	}
    }
    else
    {
	top_mask = 0;
	bottom_mask = 0;
    }

    end = buffer + width;

    /* Zero fill to the left of the image */
    while (buffer < end && x < pixman_fixed_minus_1)
    {
	*buffer++ = 0;
	x += ux;
	x_top += ux_top;
	x_bottom += ux_bottom;
	mask += mask_inc;
    }

    /* Left edge
     */
    while (buffer < end && x < 0)
    {
	xuint32_t tr, br;
	xint32_t distx;

	tr = top_row[pixman_fixed_to_int (x_top) + 1] | top_mask;
	br = bottom_row[pixman_fixed_to_int (x_bottom) + 1] | bottom_mask;

	distx = pixman_fixed_to_bilinear_weight (x);

	*buffer++ = bilinear_interpolation (0, tr, 0, br, distx, disty);

	x += ux;
	x_top += ux_top;
	x_bottom += ux_bottom;
	mask += mask_inc;
    }

    /* Main part */
    w = pixman_int_to_fixed (bits->width - 1);

    while (buffer < end  &&  x < w)
    {
	if (*mask)
	{
	    xuint32_t tl, tr, bl, br;
	    xint32_t distx;

	    tl = top_row [pixman_fixed_to_int (x_top)] | top_mask;
	    tr = top_row [pixman_fixed_to_int (x_top) + 1] | top_mask;
	    bl = bottom_row [pixman_fixed_to_int (x_bottom)] | bottom_mask;
	    br = bottom_row [pixman_fixed_to_int (x_bottom) + 1] | bottom_mask;

	    distx = pixman_fixed_to_bilinear_weight (x);

	    *buffer = bilinear_interpolation (tl, tr, bl, br, distx, disty);
	}

	buffer++;
	x += ux;
	x_top += ux_top;
	x_bottom += ux_bottom;
	mask += mask_inc;
    }

    /* Right Edge */
    w = pixman_int_to_fixed (bits->width);
    while (buffer < end  &&  x < w)
    {
	if (*mask)
	{
	    xuint32_t tl, bl;
	    xint32_t distx;

	    tl = top_row [pixman_fixed_to_int (x_top)] | top_mask;
	    bl = bottom_row [pixman_fixed_to_int (x_bottom)] | bottom_mask;

	    distx = pixman_fixed_to_bilinear_weight (x);

	    *buffer = bilinear_interpolation (tl, 0, bl, 0, distx, disty);
	}

	buffer++;
	x += ux;
	x_top += ux_top;
	x_bottom += ux_bottom;
	mask += mask_inc;
    }

    /* Zero fill to the left of the image */
    while (buffer < end)
	*buffer++ = 0;

    return iter->buffer;
}

typedef xuint32_t (* convert_pixel_t) (const xuint8_t *row, int x);

static force_inline void
bits_image_fetch_separable_convolution_affine (pixman_image_t * image,
					       int              offset,
					       int              line,
					       int              width,
					       xuint32_t *       buffer,
					       const xuint32_t * mask,

					       convert_pixel_t	convert_pixel,
					       pixman_format_code_t	format,
					       pixman_repeat_t	repeat_mode)
{
    bits_image_t *bits = &image->bits;
    pixman_fixed_t *params = image->common.filter_params;
    int cwidth = pixman_fixed_to_int (params[0]);
    int cheight = pixman_fixed_to_int (params[1]);
    int x_off = ((cwidth << 16) - pixman_fixed_1) >> 1;
    int y_off = ((cheight << 16) - pixman_fixed_1) >> 1;
    int x_phase_bits = pixman_fixed_to_int (params[2]);
    int y_phase_bits = pixman_fixed_to_int (params[3]);
    int x_phase_shift = 16 - x_phase_bits;
    int y_phase_shift = 16 - y_phase_bits;
    pixman_fixed_t vx, vy;
    pixman_fixed_t ux, uy;
    pixman_vector_t v;
    int k;

    /* reference point is the center of the pixel */
    v.vector[0] = pixman_int_to_fixed (offset) + pixman_fixed_1 / 2;
    v.vector[1] = pixman_int_to_fixed (line) + pixman_fixed_1 / 2;
    v.vector[2] = pixman_fixed_1;

    if (!pixman_transform_point_3d (image->common.transform, &v))
	return;

    ux = image->common.transform->matrix[0][0];
    uy = image->common.transform->matrix[1][0];

    vx = v.vector[0];
    vy = v.vector[1];

    for (k = 0; k < width; ++k)
    {
	pixman_fixed_t *y_params;
	int satot, srtot, sgtot, sbtot;
	pixman_fixed_t x, y;
	xint32_t x1, x2, y1, y2;
	xint32_t px, py;
	int i, j;

	if (mask && !mask[k])
	    goto next;

	/* Round x and y to the middle of the closest phase before continuing. This
	 * ensures that the convolution matrix is aligned right, since it was
	 * positioned relative to a particular phase (and not relative to whatever
	 * exact fraction we happen to get here).
	 */
	x = ((vx >> x_phase_shift) << x_phase_shift) + ((1 << x_phase_shift) >> 1);
	y = ((vy >> y_phase_shift) << y_phase_shift) + ((1 << y_phase_shift) >> 1);

	px = (x & 0xffff) >> x_phase_shift;
	py = (y & 0xffff) >> y_phase_shift;

	x1 = pixman_fixed_to_int (x - pixman_fixed_e - x_off);
	y1 = pixman_fixed_to_int (y - pixman_fixed_e - y_off);
	x2 = x1 + cwidth;
	y2 = y1 + cheight;

	satot = srtot = sgtot = sbtot = 0;

	y_params = params + 4 + (1 << x_phase_bits) * cwidth + py * cheight;

	for (i = y1; i < y2; ++i)
	{
	    pixman_fixed_t fy = *y_params++;

	    if (fy)
	    {
		pixman_fixed_t *x_params = params + 4 + px * cwidth;

		for (j = x1; j < x2; ++j)
		{
		    pixman_fixed_t fx = *x_params++;
		    int rx = j;
		    int ry = i;
		    
		    if (fx)
		    {
			pixman_fixed_t f;
			xuint32_t pixel, mask;
			xuint8_t *row;

			mask = PIXMAN_FORMAT_A (format)? 0 : 0xff000000;

			if (repeat_mode != PIXMAN_REPEAT_NONE)
			{
			    repeat (repeat_mode, &rx, bits->width);
			    repeat (repeat_mode, &ry, bits->height);

			    row = (xuint8_t *)bits->bits + bits->rowstride * 4 * ry;
			    pixel = convert_pixel (row, rx) | mask;
			}
			else
			{
			    if (rx < 0 || ry < 0 || rx >= bits->width || ry >= bits->height)
			    {
				pixel = 0;
			    }
			    else
			    {
				row = (xuint8_t *)bits->bits + bits->rowstride * 4 * ry;
				pixel = convert_pixel (row, rx) | mask;
			    }
			}

			f = ((pixman_fixed_32_32_t)fx * fy + 0x8000) >> 16;
			srtot += (int)RED_8 (pixel) * f;
			sgtot += (int)GREEN_8 (pixel) * f;
			sbtot += (int)BLUE_8 (pixel) * f;
			satot += (int)ALPHA_8 (pixel) * f;
		    }
		}
	    }
	}

	satot = (satot + 0x8000) >> 16;
	srtot = (srtot + 0x8000) >> 16;
	sgtot = (sgtot + 0x8000) >> 16;
	sbtot = (sbtot + 0x8000) >> 16;

	satot = CLIP (satot, 0, 0xff);
	srtot = CLIP (srtot, 0, 0xff);
	sgtot = CLIP (sgtot, 0, 0xff);
	sbtot = CLIP (sbtot, 0, 0xff);

	buffer[k] = (satot << 24) | (srtot << 16) | (sgtot << 8) | (sbtot << 0);

    next:
	vx += ux;
	vy += uy;
    }
}

static const xuint8_t zero[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };

static force_inline void
bits_image_fetch_bilinear_affine (pixman_image_t * image,
				  int              offset,
				  int              line,
				  int              width,
				  xuint32_t *       buffer,
				  const xuint32_t * mask,

				  convert_pixel_t	convert_pixel,
				  pixman_format_code_t	format,
				  pixman_repeat_t	repeat_mode)
{
    pixman_fixed_t x, y;
    pixman_fixed_t ux, uy;
    pixman_vector_t v;
    bits_image_t *bits = &image->bits;
    int i;

    /* reference point is the center of the pixel */
    v.vector[0] = pixman_int_to_fixed (offset) + pixman_fixed_1 / 2;
    v.vector[1] = pixman_int_to_fixed (line) + pixman_fixed_1 / 2;
    v.vector[2] = pixman_fixed_1;

    if (!pixman_transform_point_3d (image->common.transform, &v))
	return;

    ux = image->common.transform->matrix[0][0];
    uy = image->common.transform->matrix[1][0];

    x = v.vector[0];
    y = v.vector[1];

    for (i = 0; i < width; ++i)
    {
	int x1, y1, x2, y2;
	xuint32_t tl, tr, bl, br;
	xint32_t distx, disty;
	int width = image->bits.width;
	int height = image->bits.height;
	const xuint8_t *row1;
	const xuint8_t *row2;

	if (mask && !mask[i])
	    goto next;

	x1 = x - pixman_fixed_1 / 2;
	y1 = y - pixman_fixed_1 / 2;

	distx = pixman_fixed_to_bilinear_weight (x1);
	disty = pixman_fixed_to_bilinear_weight (y1);

	y1 = pixman_fixed_to_int (y1);
	y2 = y1 + 1;
	x1 = pixman_fixed_to_int (x1);
	x2 = x1 + 1;

	if (repeat_mode != PIXMAN_REPEAT_NONE)
	{
	    xuint32_t mask;

	    mask = PIXMAN_FORMAT_A (format)? 0 : 0xff000000;

	    repeat (repeat_mode, &x1, width);
	    repeat (repeat_mode, &y1, height);
	    repeat (repeat_mode, &x2, width);
	    repeat (repeat_mode, &y2, height);

	    row1 = (xuint8_t *)bits->bits + bits->rowstride * 4 * y1;
	    row2 = (xuint8_t *)bits->bits + bits->rowstride * 4 * y2;

	    tl = convert_pixel (row1, x1) | mask;
	    tr = convert_pixel (row1, x2) | mask;
	    bl = convert_pixel (row2, x1) | mask;
	    br = convert_pixel (row2, x2) | mask;
	}
	else
	{
	    xuint32_t mask1, mask2;
	    int bpp;

	    /* Note: PIXMAN_FORMAT_BPP() returns an unsigned value,
	     * which means if you use it in expressions, those
	     * expressions become unsigned themselves. Since
	     * the variables below can be negative in some cases,
	     * that will lead to crashes on 64 bit architectures.
	     *
	     * So this line makes sure bpp is signed
	     */
	    bpp = PIXMAN_FORMAT_BPP (format);

	    if (x1 >= width || x2 < 0 || y1 >= height || y2 < 0)
	    {
		buffer[i] = 0;
		goto next;
	    }

	    if (y2 == 0)
	    {
		row1 = zero;
		mask1 = 0;
	    }
	    else
	    {
		row1 = (xuint8_t *)bits->bits + bits->rowstride * 4 * y1;
		row1 += bpp / 8 * x1;

		mask1 = PIXMAN_FORMAT_A (format)? 0 : 0xff000000;
	    }

	    if (y1 == height - 1)
	    {
		row2 = zero;
		mask2 = 0;
	    }
	    else
	    {
		row2 = (xuint8_t *)bits->bits + bits->rowstride * 4 * y2;
		row2 += bpp / 8 * x1;

		mask2 = PIXMAN_FORMAT_A (format)? 0 : 0xff000000;
	    }

	    if (x2 == 0)
	    {
		tl = 0;
		bl = 0;
	    }
	    else
	    {
		tl = convert_pixel (row1, 0) | mask1;
		bl = convert_pixel (row2, 0) | mask2;
	    }

	    if (x1 == width - 1)
	    {
		tr = 0;
		br = 0;
	    }
	    else
	    {
		tr = convert_pixel (row1, 1) | mask1;
		br = convert_pixel (row2, 1) | mask2;
	    }
	}

	buffer[i] = bilinear_interpolation (
	    tl, tr, bl, br, distx, disty);

    next:
	x += ux;
	y += uy;
    }
}

static force_inline void
bits_image_fetch_nearest_affine (pixman_image_t * image,
				 int              offset,
				 int              line,
				 int              width,
				 xuint32_t *       buffer,
				 const xuint32_t * mask,
				 
				 convert_pixel_t	convert_pixel,
				 pixman_format_code_t	format,
				 pixman_repeat_t	repeat_mode)
{
    pixman_fixed_t x, y;
    pixman_fixed_t ux, uy;
    pixman_vector_t v;
    bits_image_t *bits = &image->bits;
    int i;

    /* reference point is the center of the pixel */
    v.vector[0] = pixman_int_to_fixed (offset) + pixman_fixed_1 / 2;
    v.vector[1] = pixman_int_to_fixed (line) + pixman_fixed_1 / 2;
    v.vector[2] = pixman_fixed_1;

    if (!pixman_transform_point_3d (image->common.transform, &v))
	return;

    ux = image->common.transform->matrix[0][0];
    uy = image->common.transform->matrix[1][0];

    x = v.vector[0];
    y = v.vector[1];

    for (i = 0; i < width; ++i)
    {
	int width, height, x0, y0;
	const xuint8_t *row;

	if (mask && !mask[i])
	    goto next;
	
	width = image->bits.width;
	height = image->bits.height;
	x0 = pixman_fixed_to_int (x - pixman_fixed_e);
	y0 = pixman_fixed_to_int (y - pixman_fixed_e);

	if (repeat_mode == PIXMAN_REPEAT_NONE &&
	    (y0 < 0 || y0 >= height || x0 < 0 || x0 >= width))
	{
	    buffer[i] = 0;
	}
	else
	{
	    xuint32_t mask = PIXMAN_FORMAT_A (format)? 0 : 0xff000000;

	    if (repeat_mode != PIXMAN_REPEAT_NONE)
	    {
		repeat (repeat_mode, &x0, width);
		repeat (repeat_mode, &y0, height);
	    }

	    row = (xuint8_t *)bits->bits + bits->rowstride * 4 * y0;

	    buffer[i] = convert_pixel (row, x0) | mask;
	}

    next:
	x += ux;
	y += uy;
    }
}

static force_inline xuint32_t
convert_a8r8g8b8 (const xuint8_t *row, int x)
{
    return *(((xuint32_t *)row) + x);
}

static force_inline xuint32_t
convert_x8r8g8b8 (const xuint8_t *row, int x)
{
    return *(((xuint32_t *)row) + x);
}

static force_inline xuint32_t
convert_a8 (const xuint8_t *row, int x)
{
    return *(row + x) << 24;
}

static force_inline xuint32_t
convert_r5g6b5 (const xuint8_t *row, int x)
{
    return convert_0565_to_0888 (*((xuint16_t *)row + x));
}

#define MAKE_SEPARABLE_CONVOLUTION_FETCHER(name, format, repeat_mode)  \
    static xuint32_t *							\
    bits_image_fetch_separable_convolution_affine_ ## name (pixman_iter_t   *iter, \
							    const xuint32_t * mask) \
    {									\
	bits_image_fetch_separable_convolution_affine (                 \
	    iter->image,                                                \
	    iter->x, iter->y++,                                         \
	    iter->width,                                                \
	    iter->buffer, mask,                                         \
	    convert_ ## format,                                         \
	    PIXMAN_ ## format,                                          \
	    repeat_mode);                                               \
									\
	return iter->buffer;                                            \
    }

#define MAKE_BILINEAR_FETCHER(name, format, repeat_mode)		\
    static xuint32_t *							\
    bits_image_fetch_bilinear_affine_ ## name (pixman_iter_t   *iter,	\
					       const xuint32_t * mask)	\
    {									\
	bits_image_fetch_bilinear_affine (iter->image,			\
					  iter->x, iter->y++,		\
					  iter->width,			\
					  iter->buffer, mask,		\
					  convert_ ## format,		\
					  PIXMAN_ ## format,		\
					  repeat_mode);			\
	return iter->buffer;						\
    }

#define MAKE_NEAREST_FETCHER(name, format, repeat_mode)			\
    static xuint32_t *							\
    bits_image_fetch_nearest_affine_ ## name (pixman_iter_t   *iter,	\
					      const xuint32_t * mask)	\
    {									\
	bits_image_fetch_nearest_affine (iter->image,			\
					 iter->x, iter->y++,		\
					 iter->width,			\
					 iter->buffer, mask,		\
					 convert_ ## format,		\
					 PIXMAN_ ## format,		\
					 repeat_mode);			\
	return iter->buffer;						\
    }

#define MAKE_FETCHERS(name, format, repeat_mode)			\
    MAKE_NEAREST_FETCHER (name, format, repeat_mode)			\
    MAKE_BILINEAR_FETCHER (name, format, repeat_mode)			\
    MAKE_SEPARABLE_CONVOLUTION_FETCHER (name, format, repeat_mode)

MAKE_FETCHERS (pad_a8r8g8b8,     a8r8g8b8, PIXMAN_REPEAT_PAD)
MAKE_FETCHERS (none_a8r8g8b8,    a8r8g8b8, PIXMAN_REPEAT_NONE)
MAKE_FETCHERS (reflect_a8r8g8b8, a8r8g8b8, PIXMAN_REPEAT_REFLECT)
MAKE_FETCHERS (normal_a8r8g8b8,  a8r8g8b8, PIXMAN_REPEAT_NORMAL)
MAKE_FETCHERS (pad_x8r8g8b8,     x8r8g8b8, PIXMAN_REPEAT_PAD)
MAKE_FETCHERS (none_x8r8g8b8,    x8r8g8b8, PIXMAN_REPEAT_NONE)
MAKE_FETCHERS (reflect_x8r8g8b8, x8r8g8b8, PIXMAN_REPEAT_REFLECT)
MAKE_FETCHERS (normal_x8r8g8b8,  x8r8g8b8, PIXMAN_REPEAT_NORMAL)
MAKE_FETCHERS (pad_a8,           a8,       PIXMAN_REPEAT_PAD)
MAKE_FETCHERS (none_a8,          a8,       PIXMAN_REPEAT_NONE)
MAKE_FETCHERS (reflect_a8,	 a8,       PIXMAN_REPEAT_REFLECT)
MAKE_FETCHERS (normal_a8,	 a8,       PIXMAN_REPEAT_NORMAL)
MAKE_FETCHERS (pad_r5g6b5,       r5g6b5,   PIXMAN_REPEAT_PAD)
MAKE_FETCHERS (none_r5g6b5,      r5g6b5,   PIXMAN_REPEAT_NONE)
MAKE_FETCHERS (reflect_r5g6b5,   r5g6b5,   PIXMAN_REPEAT_REFLECT)
MAKE_FETCHERS (normal_r5g6b5,    r5g6b5,   PIXMAN_REPEAT_NORMAL)

#define IMAGE_FLAGS							\
    (FAST_PATH_STANDARD_FLAGS | FAST_PATH_ID_TRANSFORM |		\
     FAST_PATH_BITS_IMAGE | FAST_PATH_SAMPLES_COVER_CLIP_NEAREST)

static const pixman_iter_info_t fast_iters[] = 
{
    { PIXMAN_r5g6b5, IMAGE_FLAGS, ITER_NARROW | ITER_SRC,
      _pixman_iter_init_bits_stride, fast_fetch_r5g6b5, XNULL },

    { PIXMAN_r5g6b5, FAST_PATH_STD_DEST_FLAGS,
      ITER_NARROW | ITER_DEST,
      _pixman_iter_init_bits_stride,
      fast_fetch_r5g6b5, fast_write_back_r5g6b5 },
    
    { PIXMAN_r5g6b5, FAST_PATH_STD_DEST_FLAGS,
      ITER_NARROW | ITER_DEST | ITER_IGNORE_RGB | ITER_IGNORE_ALPHA,
      _pixman_iter_init_bits_stride,
      fast_dest_fetch_noop, fast_write_back_r5g6b5 },

    { PIXMAN_a8r8g8b8,
      (FAST_PATH_STANDARD_FLAGS			|
       FAST_PATH_SCALE_TRANSFORM		|
       FAST_PATH_BILINEAR_FILTER		|
       FAST_PATH_SAMPLES_COVER_CLIP_BILINEAR),
      ITER_NARROW | ITER_SRC,
      fast_bilinear_cover_iter_init,
      XNULL, XNULL
    },

#define FAST_BILINEAR_FLAGS						\
    (FAST_PATH_NO_ALPHA_MAP		|				\
     FAST_PATH_NO_ACCESSORS		|				\
     FAST_PATH_HAS_TRANSFORM		|				\
     FAST_PATH_AFFINE_TRANSFORM		|				\
     FAST_PATH_X_UNIT_POSITIVE		|				\
     FAST_PATH_Y_UNIT_ZERO		|				\
     FAST_PATH_NONE_REPEAT		|				\
     FAST_PATH_BILINEAR_FILTER)

    { PIXMAN_a8r8g8b8,
      FAST_BILINEAR_FLAGS,
      ITER_NARROW | ITER_SRC,
      XNULL, bits_image_fetch_bilinear_no_repeat_8888, XNULL
    },

    { PIXMAN_x8r8g8b8,
      FAST_BILINEAR_FLAGS,
      ITER_NARROW | ITER_SRC,
      XNULL, bits_image_fetch_bilinear_no_repeat_8888, XNULL
    },

#define GENERAL_BILINEAR_FLAGS						\
    (FAST_PATH_NO_ALPHA_MAP		|				\
     FAST_PATH_NO_ACCESSORS		|				\
     FAST_PATH_HAS_TRANSFORM		|				\
     FAST_PATH_AFFINE_TRANSFORM		|				\
     FAST_PATH_BILINEAR_FILTER)

#define GENERAL_NEAREST_FLAGS						\
    (FAST_PATH_NO_ALPHA_MAP		|				\
     FAST_PATH_NO_ACCESSORS		|				\
     FAST_PATH_HAS_TRANSFORM		|				\
     FAST_PATH_AFFINE_TRANSFORM		|				\
     FAST_PATH_NEAREST_FILTER)

#define GENERAL_SEPARABLE_CONVOLUTION_FLAGS				\
    (FAST_PATH_NO_ALPHA_MAP            |				\
     FAST_PATH_NO_ACCESSORS            |				\
     FAST_PATH_HAS_TRANSFORM           |				\
     FAST_PATH_AFFINE_TRANSFORM        |				\
     FAST_PATH_SEPARABLE_CONVOLUTION_FILTER)
    
#define SEPARABLE_CONVOLUTION_AFFINE_FAST_PATH(name, format, repeat)   \
    { PIXMAN_ ## format,						\
      GENERAL_SEPARABLE_CONVOLUTION_FLAGS | FAST_PATH_ ## repeat ## _REPEAT, \
      ITER_NARROW | ITER_SRC,						\
      XNULL, bits_image_fetch_separable_convolution_affine_ ## name, XNULL \
    },

#define BILINEAR_AFFINE_FAST_PATH(name, format, repeat)			\
    { PIXMAN_ ## format,						\
      GENERAL_BILINEAR_FLAGS | FAST_PATH_ ## repeat ## _REPEAT,		\
      ITER_NARROW | ITER_SRC,						\
      XNULL, bits_image_fetch_bilinear_affine_ ## name, XNULL,		\
    },

#define NEAREST_AFFINE_FAST_PATH(name, format, repeat)			\
    { PIXMAN_ ## format,						\
      GENERAL_NEAREST_FLAGS | FAST_PATH_ ## repeat ## _REPEAT,		\
      ITER_NARROW | ITER_SRC,						\
      XNULL, bits_image_fetch_nearest_affine_ ## name, XNULL		\
    },

#define AFFINE_FAST_PATHS(name, format, repeat)				\
    SEPARABLE_CONVOLUTION_AFFINE_FAST_PATH(name, format, repeat)	\
    BILINEAR_AFFINE_FAST_PATH(name, format, repeat)			\
    NEAREST_AFFINE_FAST_PATH(name, format, repeat)
    
    AFFINE_FAST_PATHS (pad_a8r8g8b8, a8r8g8b8, PAD)
    AFFINE_FAST_PATHS (none_a8r8g8b8, a8r8g8b8, NONE)
    AFFINE_FAST_PATHS (reflect_a8r8g8b8, a8r8g8b8, REFLECT)
    AFFINE_FAST_PATHS (normal_a8r8g8b8, a8r8g8b8, NORMAL)
    AFFINE_FAST_PATHS (pad_x8r8g8b8, x8r8g8b8, PAD)
    AFFINE_FAST_PATHS (none_x8r8g8b8, x8r8g8b8, NONE)
    AFFINE_FAST_PATHS (reflect_x8r8g8b8, x8r8g8b8, REFLECT)
    AFFINE_FAST_PATHS (normal_x8r8g8b8, x8r8g8b8, NORMAL)
    AFFINE_FAST_PATHS (pad_a8, a8, PAD)
    AFFINE_FAST_PATHS (none_a8, a8, NONE)
    AFFINE_FAST_PATHS (reflect_a8, a8, REFLECT)
    AFFINE_FAST_PATHS (normal_a8, a8, NORMAL)
    AFFINE_FAST_PATHS (pad_r5g6b5, r5g6b5, PAD)
    AFFINE_FAST_PATHS (none_r5g6b5, r5g6b5, NONE)
    AFFINE_FAST_PATHS (reflect_r5g6b5, r5g6b5, REFLECT)
    AFFINE_FAST_PATHS (normal_r5g6b5, r5g6b5, NORMAL)

    { PIXMAN_null },
};

pixman_implementation_t *
_pixman_implementation_create_fast_path (pixman_implementation_t *fallback)
{
    pixman_implementation_t *imp = _pixman_implementation_create (fallback, c_fast_paths);

    imp->fill = fast_path_fill;
    imp->iter_info = fast_iters;

    return imp;
}
