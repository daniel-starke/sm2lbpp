/**
 * @file sm2lbpp.h
 * @author Daniel Starke
 * @date 2023-05-10
 * @version 2023-05-18
 *
 * DISCLAIMER
 * This file has no copyright assigned and is placed in the Public Domain.
 * All contributions are also assumed to be in the Public Domain.
 * Other contributions are not permitted.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef __SM2LBPP_H__
#define __SM2LBPP_H__

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <png.h>
#include "target.h"
#define NANOSVG_IMPLEMENTATION
#include "../nanosvg/nanosvg.h"
#define NANOSVGRAST_IMPLEMENTATION
#include "../nanosvg/nanosvgrast.h"
#include "parser.h"
#include "tchar.h"
#include "version.h"

/**
 * Defines a color value from the red, green, blue and alpha component.
 *
 * @param r - red value (0..255)
 * @param g - green value (0..255)
 * @param b - blue value (0..255)
 * @param a - alpha value (0..255) with 255 being opaque
 * @return ABGR
 */
#define COLOR(r, g, b, a) ((a << 24) | (b << 16) | (g << 8) | (r))

/** Input line buffer size(in bytes). */
#define LINE_BUFFER_SIZE 0x80000UL

/** Initial point vector size (in bytes). */
#define VEC_INIT_SIZE 0x10000UL

/** Maximum point vector grow size (in bytes). */
#define VEC_MAX_GROW_SIZE 0x8000000UL

/** Output image pixel width. */
#define IMAGE_WIDTH 300

/** Output image pixel height. */
#define IMAGE_HEIGHT 150

/** Laser point diameter in workspace millimeters. This is used as stroke width. */
#define STROKE_WIDTH 0.3f

/** Background color in ABGR. The alpha channel is discarded. */
#define BACKGROUND_COLOR COLOR(255, 255, 255, 255)

/** Stroke color in ABGR. */
#define STROKE_COLOR COLOR(0, 0, 0, 255)

/** Fill color in ABGR. */
#define FILL_COLOR COLOR(255, 255, 255, 0) /* white, fully transparent */

/** Horizontal border clearance in workspace millimeters. */
#define BORDER_WIDTH 1.0f

/** Vertical border clearance in workspace millimeters. */
#define BORDER_HEIGHT 1.0f


/** Enumeration of possible error values. */
typedef enum {
	MSGT_SUCCESS = 0,
	MSGT_ERR_NO_MEM,
	MSGT_ERR_FILE_NOT_FOUND,
	MSGT_ERR_FILE_OPEN,
	MSGT_ERR_FILE_READ,
	MSGT_ERR_FILE_CREATE,
	MSGT_ERR_FILE_WRITE,
	MSGT_ERR_PNG,
	MSGT_WARN_NO_TOTAL_LINES,
	MSGT_WARN_NO_TOTAL_LINES_LINE,
	MSGT_INFO_PRESS_ENTER,
	MSG_COUNT
} tMessage;


/** Error callback type. */
typedef int (* tCallback)(const tMessage msg, const TCHAR * file, const size_t line);


/** Defines the structure for a point vector. */
typedef struct {
	size_t start;    /**< The start size of the vector in number of points. */
	size_t size;     /**< The current size of the vector in number of points. */
	size_t capacity; /**< The maximum capacity of the vector in number of points. */
	float * data;    /**< The pointed memory of the point vector. */
} tPointVec;


/** Defines the structure which holds the data of a PNG image. */
typedef struct {
	size_t size;    /**< The current size of the pointed data. */
	png_bytep data; /**< The pointed memory of the PNG. */
} tPng;


extern FILE * fin;
extern FILE * fout;
extern FILE * ferr;
extern const TCHAR * fmsg[MSG_COUNT];


/* helper functions */
void printHelp(void);
int processFile(const TCHAR * file, const tCallback cb);
int errorCallback(const tMessage msg, const TCHAR * file, const size_t line);


#endif /* __SM2LBPP_H__ */
