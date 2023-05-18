/**
 * @file sm2lbpp.c
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
#include "sm2lbpp.h"
#include "mingw-unicode.h"


FILE * fin = NULL;
FILE * fout = NULL;
FILE * ferr = NULL;


const TCHAR * fmsg[MSG_COUNT] = {
	/* MSGT_SUCCESS                  */ _T(""), /* never used for output */
	/* MSGT_ERR_NO_MEM               */ _T("Error: Failed to allocate memory.\n"),
	/* MSGT_ERR_FILE_NOT_FOUND       */ _T("Error: Input file not found.\n"),
	/* MSGT_ERR_FILE_OPEN            */ _T("Error: Failed to open file for reading.\n"),
	/* MSGT_ERR_FILE_READ            */ _T("Error: Failed to read data from file.\n"),
	/* MSGT_ERR_FILE_CREATE          */ _T("Error: Failed to create file for writing.\n"),
	/* MSGT_ERR_FILE_WRITE           */ _T("Error: Failed to write data to file.\n"),
	/* MSGT_ERR_PNG                  */ _T("Error: Failed to encode PNG image.\n"),
	/* MSGT_WARN_NO_TOTAL_LINES      */ _T("Warning: 'file_total_lines' was not found.\n"),
	/* MSGT_WARN_NO_TOTAL_LINES_LINE */ _T("Warning: Line with 'file_total_lines' is unterminated.\n"),
	/* MSGT_INFO_PRESS_ENTER         */ _T("Press ENTER to exit.\n")
};


/**
 * Main entry point.
 */
int _tmain(int argc, TCHAR ** argv) {
	/* set the output file descriptors */
	fin  = stdin;
	fout = stdout;
	ferr = stderr;

#ifdef UNICODE
	/* http://msdn.microsoft.com/en-us/library/z0kc8e3z(v=vs.80).aspx */
	if (_isatty(_fileno(fout))) {
		_setmode(_fileno(fout), _O_U16TEXT);
	} else {
		_setmode(_fileno(fout), _O_U8TEXT);
	}
	if (_isatty(_fileno(ferr))) {
		_setmode(_fileno(ferr), _O_U16TEXT);
	} else {
		_setmode(_fileno(ferr), _O_U8TEXT);
	}
#endif /* UNICODE */

	if (argc < 2) {
		printHelp();
		return EXIT_FAILURE;
	}

	return (processFile(argv[1], &errorCallback) == 1) ? EXIT_SUCCESS : EXIT_FAILURE;
}


/**
 * Write the help for this application to standard out.
 */
void printHelp(void) {
	_ftprintf(ferr,
	_T("sm2lbpp <g-code file>\n")
	_T("\n")
	_T("sm2lbpp ") _T2(PROGRAM_VERSION_STR) _T("\n")
	_T("https://github.com/daniel-starke/sm2lbpp\n")
	);
}


/**
 * Converts the given token into a unsigned integer value.
 *
 * @param[in] aToken - token to convert
 * @return integer value from the token
 */
static unsigned int p_uint(const tPToken * aToken) {
	if (aToken->start == NULL || aToken->length <= 0) return 0;
	unsigned int val = 0;
	for (size_t i = 0; i < aToken->length; i++) {
		const char ch = aToken->start[i];
		switch (ch) {
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			val = (val * 10) + ((unsigned int)(ch - '0'));
			break;
		default:
			goto endOfNumber;
		}
	}
endOfNumber:
	return val;
}


/**
 * Converts the given token into a float value. Simple float values are assumed.
 *
 * @param[in] aToken - token to convert
 * @return float value from the token
 */
static float p_float(const tPToken * aToken) {
	if (aToken->start == NULL || aToken->length <= 0) return 0.0f;
	size_t val = 0;
	size_t frac = 0;
	float fracDiv = 1.0f;
	int isFrac = 0;
	size_t i = 0;
	int sign = 1;
	if (aToken->start[0] == '-') {
		sign = -1;
		i++;
	}
	for (; i < aToken->length; i++) {
		const char ch = aToken->start[i];
		switch (ch) {
		case '0': case '1': case '2': case '3': case '4':
		case '5': case '6': case '7': case '8': case '9':
			if (isFrac == 0) {
				val = (val * 10) + ((size_t)(ch - '0'));
			} else {
				frac = (frac * 10) + ((size_t)(ch - '0'));
				fracDiv *= 10.0f;
			}
			break;
		case '.':
			isFrac = 1;
			break;
		default:
			goto endOfNumber;
		}
	}
endOfNumber:
	return (float)sign * ((float)val + ((float)frac / fracDiv));
}


/**
 * Deletes the given SVG image.
 *
 * @param[in,out] svg - SVG to delete
 */
static void deleteSvg(NSVGimage * svg) {
	NSVGshape * snext, * shape;
	NSVGpath * pnext, * path;
	if (svg == NULL) {
		return;
	}
	shape = svg->shapes;
	while (shape != NULL) {
		snext = shape->next;
		path = shape->paths;
		while (path != NULL) {
			pnext = path->next;
			free(path);
			path = pnext;
		}
		free(shape);
		shape = snext;
	}
	free(svg);
}


/**
 * Creates and initializes a nanosvg shape.
 *
 * @return created shape or NULL on allocation error
 */
static NSVGshape * createShape(void) {
	NSVGshape * shape = (NSVGshape *)malloc(sizeof(NSVGshape));
	if (shape == NULL) return NULL;
	memset(shape, 0, sizeof(NSVGshape));
	shape->opacity = 1.0; /* opaque */;
	shape->strokeWidth = STROKE_WIDTH;
	shape->strokeLineJoin = NSVG_JOIN_ROUND;
	shape->strokeLineCap = NSVG_CAP_ROUND;
	shape->miterLimit = 4.0f;
	shape->fillRule = NSVG_FILLRULE_NONZERO;
	shape->flags = NSVG_FLAGS_VISIBLE;
	shape->fill.type = NSVG_PAINT_COLOR;
	shape->fill.value.color = FILL_COLOR;
	shape->stroke.type = NSVG_PAINT_COLOR;
	shape->stroke.value.color = STROKE_COLOR;
	/* gradient transformation matrix: identity transformation (unused) */
	shape->xform[0] = 1.0f;
	shape->xform[1] = 0.0f;
	shape->xform[2] = 0.0f;
	shape->xform[3] = 1.0f;
	shape->xform[4] = 0.0f;
	shape->xform[5] = 0.0f;
	return shape;
}


/**
 * Allocates and/or growths the given point vector if no more elements
 * can be added.
 *
 * @param[in,out] vec - point vector to use (NULL to allocate)
 * @return The vector on success, or NULL on allocation/reallocation error.
 */
static tPointVec * growPointVec(tPointVec * vec) {
	if (vec == NULL) {
		vec = (tPointVec *)malloc(sizeof(tPointVec));
		if (vec == NULL) {
			return NULL;
		}
		memset(vec, 0, sizeof(tPointVec));
	}
	if (vec->data == NULL) {
		vec->capacity = (size_t)(VEC_INIT_SIZE / (2 * sizeof(float)));
		vec->data = (float *)malloc(2 * vec->capacity * sizeof(float));
		if (vec->data == NULL) {
			free(vec);
			return NULL;
		}
	}
	if (vec->size >= vec->capacity) {
		/* grow vector capacity if insufficient */
		if (vec->capacity <= (size_t)(VEC_MAX_GROW_SIZE / (2 * sizeof(float)))) {
			vec->capacity *= 2;
		} else {
			vec->capacity += (size_t)(VEC_MAX_GROW_SIZE / (2 * sizeof(float)));
		}
		float * newData = (float *)realloc(vec->data, 2 * vec->capacity * sizeof(float));
		if (newData == NULL) {
			free(vec->data);
			free(vec);
			return NULL;
		}
		vec->data = newData;
	}
	return vec;
}


/**
 * Deletes the given point vector.
 *
 * @param[in,out] vec - vector to delete
 */
static void deletePointVec(tPointVec * vec) {
	if (vec == NULL) {
		return;
	}
	if (vec->data != NULL) {
		free(vec->data);
	}
	free(vec);
}


/**
 * Adds a point to the given point vector.
 *
 * @param[in,out] vec - point vector to use (NULL to allocate)
 * @param[in] x - x coordinate of the point to add
 * @param[in] y - y coordinate of the point to add
 * @return The vector on success, or NULL on allocation/reallocation error.
 */
static tPointVec * addPoint(tPointVec * vec, const float x, const float y) {
	vec = growPointVec(vec);
	if (vec == NULL) {
		return NULL;
	}
	float * pts = vec->data + (2 * vec->size);
	pts[0] = x;
	pts[1] = y;
	vec->size++;
	return vec;
}


/**
 * Adds the cubic bezier curve parameters for a straight line to the
 * given point vector.
 *
 * @param[in] vec - point vector to use (NULL to allocate)
 * @param[in] x - x coordinate of the destination point of the line
 * @param[in] y - y coordinate of the destination point of the line
 * @return The vector on success, or NULL on allocation/reallocation error.
 */
static tPointVec * addLine(tPointVec * vec, const float x, const float y) {
	if (vec == NULL || vec->data == NULL) {
		return NULL;
	}
	const float * pts = vec->data + (2 * (vec->size - 1));
	const float px = pts[0];
	const float py = pts[1];
	const float dx = x - px;
	const float dy = y - py;
	vec = addPoint(vec, px + (dx / 3.0f), py + (dy / 3.0f));
	if (vec == NULL) {
		return NULL;
	}
	vec = addPoint(vec, x - (dx / 3.0f), y - (dy / 3.0f));
	if (vec == NULL) {
		return NULL;
	}
	return addPoint(vec, x, y);
}


/**
 * Adds a new path from the passed point vector to the given one.
 * This also clear the passed point vector on success.
 *
 * @param[in,out] pathPtr - pointer to previous path handle
 * @param[in,out] points - point vector to add
 * @return The added path on success, else NULL on allocation error.
 */
static NSVGpath * pointsToPath(NSVGpath ** pathPtr, tPointVec * points) {
	if (pathPtr == NULL || points == NULL || points->data == NULL || (points->size - points->start) <= 1) {
		return NULL; /* invalid value */
	}
	NSVGpath * path = (NSVGpath *)malloc(sizeof(NSVGpath));
	if (path == NULL) {
		return NULL;
	}
	memset(path, 0, sizeof(NSVGpath));
	/* Only remember start position here.
	 * The actual offset to points->data will be set later
	 * via setPointsDataOffset() to avoid invalid pointers
	 * if realloc() returns a new location in growPointVec().
	 */
	path->pts = (float *)(2 * points->start);
	path->npts = (int)(points->size - points->start);
	points->start = points->size; /* reset start */
	/* set previous path's next pointer */
	*pathPtr = path;
	return path;
}


/**
 * Updates the path->pts offset to ptsBase to correctly map the
 * pointers after realloc() relocation in growPointVec().
 *
 * @param[in] path - linked list of path objects
 * @param[in] ptsBase - new base pointer for path->pts
 */
static void setPointsDataOffset(NSVGpath * path, float * ptsBase) {
	while (path != NULL) {
		path->pts = ptsBase + (ptrdiff_t)(path->pts);
		path = path->next;
	}
}


/**
 * Sets the bounds to the given path with all its points.
 *
 * @param[in,out] path - path to set bounds for
 */
static void setPathBounds(NSVGpath * path) {
	if (path == NULL) {
		return;
	}
	/* calculate bounds, assuming all straight lines */
	for (int i = 0; (i + 3) < path->npts; i += 3) {
		const float * curve = path->pts + (i * 2);
		if (i == 0) {
			path->bounds[0] = curve[0];
			path->bounds[1] = curve[1];
			path->bounds[2] = curve[6];
			path->bounds[3] = curve[7];
		} else {
			path->bounds[0] = PCF_MIN(path->bounds[0], curve[0]);
			path->bounds[1] = PCF_MIN(path->bounds[1], curve[1]);
			path->bounds[2] = PCF_MAX(path->bounds[2], curve[6]);
			path->bounds[3] = PCF_MAX(path->bounds[3], curve[7]);
		}
	}
}


/**
 * Clamps and converts the given float to a valid
 * color value. The value range is 0..255.
 *
 * @param[in] val - value to convert
 * @return converted value
 */
static unsigned char toColor(const float val) {
	if (val > 255.0f) {
		return 255;
	} else if (val < 0.0f) {
		return 0;
	} else {
		return (unsigned char)val;
	}
}


/**
 * Blends the pixel with the given color according to the
 * alpha value of the pixel. All color values are RGBA.
 *
 * @param[in,out] pixel - pixel to blend (modified in-place)
 * @param[in] color - color to blend with
 */
static void blendByAlpha(unsigned char * pixel, unsigned int color) {
	const float a = ((float)pixel[3]) / 255.0f;
	for (size_t i = 0; i < 3; i++) {
		float x = (float)(unsigned char)(color >> (8 * i));
		float y = (float)(*pixel);
		*pixel++ = toColor(x + ((y - x) * a) + 0.5f);
	}
	*pixel = 255; /* opaque */
}


/**
 * Adds the given PNG data block to the associated PNG object.
 *
 * @param[in] pngPtr - context
 * @param[in] data - data to add
 * @param[in] length - data length in number of bytes
 */
static void pngWriteData(png_structp pngPtr, png_bytep data, png_size_t length) {
	tPng * png = (tPng *)png_get_io_ptr(pngPtr);
	size_t newSize = png->size + length;
	if (png->data != NULL) {
		png->data = (png_bytep)realloc(png->data, newSize);
	} else {
		png->data = (png_bytep)malloc(newSize);
	}
	if (png->data == NULL) {
		png_error(pngPtr, "Failed to allocate memory.");
	}
	memcpy(png->data + png->size, data, length);
	png->size = newSize;
}


/**
 * Writes the passed image rows to the given PNG memory buffer.
 *
 * @param[in] imgRows - image row pointers
 * @param[in] png - PNG memory buffer
 * @return 1 on success
 * @return 0 on allocation error
 * @return -1 on PNG internal error
 */
static int imgToPng(png_bytepp imgRows, tPng * png) {
	int res = 0;
	png_structp pngPtr = NULL;
	png_infop pngInfoPtr = NULL;

	pngPtr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
	if (pngPtr == NULL) goto onError;
	pngInfoPtr = png_create_info_struct(pngPtr);
	if (pngInfoPtr == NULL) goto onError;
	if (setjmp(png_jmpbuf(pngPtr)) != 0) {
		res = -1;
		goto onError;
	}
	png_set_IHDR(
		pngPtr, pngInfoPtr, IMAGE_WIDTH, IMAGE_HEIGHT, 8, PNG_COLOR_TYPE_RGB_ALPHA,
		PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE
	);
	png_set_rows(pngPtr, pngInfoPtr, imgRows);
	png_set_write_fn(pngPtr, png, &pngWriteData, NULL);
	png_write_png(pngPtr, pngInfoPtr, PNG_TRANSFORM_IDENTITY, NULL);

	res = 1;
onError:
	if (pngInfoPtr != NULL) {
		png_destroy_write_struct(&pngPtr, &pngInfoPtr);
	} else if (pngPtr != NULL) {
		png_destroy_write_struct(&pngPtr, NULL);
	}
	return res;
}


/**
 * Writes the given memory block Base64 encoded to the passed
 * file descriptor.
 *
 * @param[in,out] fp - file descriptor to write to
 * @param[in] data - pointer to the input data
 * @param[in] size - number of bytes in the given input data
 */
static void writeBase64(FILE * fp, const unsigned char * data, const size_t size) {
	static char table[65] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	const unsigned char * inPtr;
	const unsigned char * endPtr;
	char buf[4];

	endPtr = data + size;
	for (inPtr = data; (inPtr + 3) <= endPtr; inPtr += 3) {
		buf[0] = table[inPtr[0] >> 2];
		buf[1] = table[((inPtr[0] & 0x03) << 4) | (inPtr[1] >> 4)];
		buf[2] = table[((inPtr[1] & 0x0F) << 2) | (inPtr[2] >> 6)];
		buf[3] = table[inPtr[2] & 0x3F];
		fwrite(buf, sizeof(buf), 1, fp);
	}

	if (inPtr < endPtr) {
		buf[0] = table[inPtr[0] >> 2];
		if ((inPtr + 1) == endPtr) {
			buf[1] = table[(inPtr[0] & 0x03) << 4];
			buf[2] = '=';
		} else {
			buf[1] = table[((inPtr[0] & 0x03) << 4) | (inPtr[1] >> 4)];
			buf[2] = table[(inPtr[1] & 0x0F) << 2];
		}
		buf[3] = '=';
		fwrite(buf, sizeof(buf), 1, fp);
	}
}


/**
 * Processes the given LightBurn generated G-Code file and adds
 * Snapmaker 2.0 terminal compatible thumbnail data.
 *
 * @param[in] file - LightBurn generated G-Code file
 * @param[in] cb - error output callback function
 * @return 1 on success, 0 on failure, -1 if aborted by callback function
 * @see https://github.com/Snapmaker/Snapmaker2-Controller/blob/main/snapmaker/src/gcode/M3-M5.cpp#L66-L69
 */
int processFile(const TCHAR * file, const tCallback cb) {
#define ON_WARN(msg) do { \
	if (cb(msg, file, lineNr) != 1) goto onError; \
} while (0) \

#define ON_ERROR(msg) do { \
	cb(msg, file, lineNr); \
	goto onError; \
} while (0)
#define GCODE(type, num) (((unsigned int)(type) << 16) | (unsigned int)(num))
#define IS_SET(num) ((num) == (num))

	if (file == NULL || cb == NULL) return 0;
	int res = 0;
	int pwrOn = 0;
	int prevOn = 0;
	int isAbsPos = 1;
	unsigned int code = -1;
	float paramX = NAN;
	float paramY = NAN;
	float paramP = NAN;
	float paramS = NAN;
	float x = NAN;
	float y = NAN;
	float pwr = 0.0f;
	float minX = +INFINITY;
	float minY = +INFINITY;
	float maxX = -INFINITY;
	float maxY = -INFINITY;
	size_t lineNr = 1;
	char * inputBuf = NULL;
	size_t inputLen = 0;
	FILE * fp = NULL;
	NSVGimage * svg = NULL;
	NSVGshape * shape = NULL;
	NSVGpath * path = NULL;
	NSVGpath ** pathPtr = NULL;
	tPointVec * pointVec = NULL;
	NSVGrasterizer * rast = NULL;
	png_bytep img = NULL;
	png_bytepp imgRows = NULL;
	tPng png = {0};
	tPToken totalLines = {0};
	tPToken totalLinesLine = {0};
	tPToken aToken = {0};
	tPToken * valueToken = NULL;
	const char * lineStart = NULL;
	enum tState {
		ST_LINE_START,
		ST_FIND_LINE_START,
		ST_GCODE,
		ST_COMMENT,
		ST_PARAMETER_VALUE
	} state = ST_LINE_START;
#ifdef DEBUG
	static const TCHAR * stateStr[] = {
		_T("ST_LINE_START"),
		_T("ST_FIND_LINE_START"),
		_T("ST_GCODE"),
		_T("ST_COMMENT"),
		_T("ST_PARAMETER_VALUE")
	};
#endif /* DEBUG */
	enum tParam {
		P_G,
		P_M,
		P_X,
		P_Y,
		P_P,
		P_S,
		P_UNKNOWN
	} param = P_UNKNOWN;

	/* initialize SVG */
	svg = (NSVGimage *)malloc(sizeof(NSVGimage));
	if (svg == NULL) ON_ERROR(MSGT_ERR_NO_MEM);
	memset(svg, 0, sizeof(NSVGimage));
	/* initialize shape */
	svg->shapes = createShape();
	if (svg->shapes == NULL) ON_ERROR(MSGT_ERR_NO_MEM);
	shape = svg->shapes;
	pathPtr = &(shape->paths);

	/* open input file for reading */
	fp = _tfopen(file, _T("rb"));
	if (fp == NULL) ON_ERROR(MSGT_ERR_FILE_OPEN);

	/* get file size */
	fseeko64(fp, 0, SEEK_END);
	inputLen = (size_t)ftello64(fp);
	if (inputLen < 1) goto onSuccess;
	fseek(fp, 0, SEEK_SET);

	/* allocate input buffer from file data */
	inputBuf = (char *)malloc(inputLen);
	if (inputBuf == NULL) ON_ERROR(MSGT_ERR_NO_MEM);
	if (fread(inputBuf, inputLen, 1, fp) < 1) ON_ERROR(MSGT_ERR_FILE_READ);

	/* close input file */
	fclose(fp);
	fp = NULL;

	/* parse tokens */
	lineStart = inputBuf;
	for (const char * it = inputBuf, * endIt = inputBuf + inputLen; it < endIt; it++) {
		const char ch = *it;
#ifdef DEBUG
		_ftprintf(ferr, _T("%u:%s: '%c'"), (unsigned)lineNr, stateStr[(int)state], ch);
		if (aToken.start != NULL) {
#ifdef UNICODE
			_ftprintf(ferr, _T(", token: \"%.*S\""), (unsigned)aToken.length, aToken.start);
#else /* not UNICODE */
			_ftprintf(ferr, _T(", token: \"%.*s\""), (unsigned)aToken.length, aToken.start);
#endif /* not UNICODE */
		}
		if (valueToken != NULL && valueToken->start != NULL) {
#ifdef UNICODE
			_ftprintf(ferr, _T(", value: \"%.*S\""), (unsigned)valueToken->length, valueToken->start);
#else /* not UNICODE */
			_ftprintf(ferr, _T(", value: \"%.*s\""), (unsigned)valueToken->length, valueToken->start);
#endif /* not UNICODE */
		}
		_ftprintf(ferr, _T("\n"));
#endif /* DEBUG */
		switch (state) {
		case ST_LINE_START:
			 if (ch == ';') {
				/* comment */
				memset(&aToken, 0, sizeof(aToken));
				state = ST_COMMENT;
			} else if (ch == 'G' || ch == 'M') {
				/* Gcode */
				param = (ch == 'G') ? P_G : P_M;
				paramX = NAN;
				paramY = NAN;
				paramP = NAN;
				paramS = NAN;
				aToken.start = it + 1;
				aToken.length = 0;
				state = ST_GCODE;
			} else if (isspace(ch) == 0) {
				/* code */
				state = ST_FIND_LINE_START;
			}
			/* spaces */
			break;
		case ST_FIND_LINE_START:
			if (ch == '\n') {
				/* new line */
				state = ST_LINE_START;
			}
			break;
		case ST_GCODE:
			if (isdigit(ch) != 0 || (param != P_G && param != P_M && (ch == '.' || (aToken.length == 0 && ch == '-')))) {
				/* number */
				aToken.length++;
			} else if (ch == 'X') {
				param = P_X;
				aToken.start = it + 1;
				aToken.length = 0;
			} else if (ch == 'Y') {
				param = P_Y;
				aToken.start = it + 1;
				aToken.length = 0;
			} else if (ch == 'P') {
				param = P_P;
				aToken.start = it + 1;
				aToken.length = 0;
			} else if (ch == 'S') {
				param = P_S;
				aToken.start = it + 1;
				aToken.length = 0;
			} else {
				/* end of token */
				switch (param) {
				case P_G:
					code = GCODE('G', p_uint(&aToken));
					break;
				case P_M:
					code = GCODE('M', p_uint(&aToken));
					break;
				case P_X:
					paramX = p_float(&aToken);
					break;
				case P_Y:
					paramY = p_float(&aToken);
					break;
				case P_P:
					paramP = p_float(&aToken);
					break;
				case P_S:
					paramS = p_float(&aToken);
					break;
				default:
					break;
				}
				param = P_UNKNOWN;
				if (ch == '\n' || ch == ';') {
					/* new line or start of comment */
					switch (code) {
					case GCODE('G', 0): /* linear move */
					case GCODE('G', 1): /* linear move */
						if (pwrOn != 0 && pwr > 0.0f && prevOn == 0) {
							/* powered move after non-powered move */
							if ( IS_SET(x) ) {
								if (minX > x) {
									minX = x;
								}
								if (maxX < x) {
									maxX = x;
								}
							}
							if ( IS_SET(y) ) {
								if (minY > y) {
									minY = y;
								}
								if (maxY < y) {
									maxY = y;
								}
							}
							if (IS_SET(x) && IS_SET(y)) {
								/* powered move after non-powered move */
								/* add first point */
								pointVec = addPoint(pointVec, x, y);
								if (pointVec == NULL) ON_ERROR(MSGT_ERR_NO_MEM);
							}
							prevOn = 1;
						}
						/* calculate new position */
						if ( IS_SET(paramX) ) {
							if (isAbsPos != 0) {
								x = paramX;
							} else {
								x += paramX;
							}
						}
						if ( IS_SET(paramY) ) {
							if (isAbsPos != 0) {
								y = paramY;
							} else {
								y += paramY;
							}
						}
						if (pwrOn != 0 && pwr > 0.0f) {
							/* powered move */
							if ( IS_SET(paramX) ) {
								if (minX > x) {
									minX = x;
								}
								if (maxX < x) {
									maxX = x;
								}
							}
							if ( IS_SET(paramY) ) {
								if (minY > y) {
									minY = y;
								}
								if (maxY < y) {
									maxY = y;
								}
							}
							/* add line to new point */
							pointVec = addLine(pointVec, x, y);
							if (pointVec == NULL) ON_ERROR(MSGT_ERR_NO_MEM);
							prevOn = 1;
						} else if (prevOn != 0) {
							/* non-powered move after powered move */
							if (pointVec != NULL && (pointVec->size - pointVec->start) > 1) {
								/* move completed path to shape */
								path = pointsToPath(pathPtr, pointVec);
								if (path == NULL) ON_ERROR(MSGT_ERR_NO_MEM);
								pathPtr = &(path->next);
							} else if (pointVec != NULL) {
								/* reset start */
								pointVec->start = pointVec->size;
							}
							prevOn = 0;
						}
						break;
					case GCODE('G', 90): /* absolute positioning */
						isAbsPos = 1;
						break;
					case GCODE('G', 91): /* relative positioning */
						isAbsPos = 0;
						break;
					case GCODE('M', 3): /* laser on */
						if ( IS_SET(paramP) ) {
							pwr = paramP;
						} else if ( IS_SET(paramS) ) {
							pwr = (paramS * 100.0f) / 255.0f;
						}
						pwrOn = 1;
						break;
					case GCODE('M', 5): /* laser off */
						pwr = 0.0f;
						pwrOn = 0;
						break;
					default:
						break;
					}
					if (ch == '\n') {
						/* new line */
						state = ST_LINE_START;
					} else {
						/* comment */
						memset(&aToken, 0, sizeof(aToken));
						state = ST_COMMENT;
					}
				}
			}
			break;
		case ST_COMMENT:
			if (ch == '\n') {
				/* end of comment line */
				state = ST_LINE_START;
			} else if (aToken.start == NULL) {
				if (isspace(ch) == 0) {
					/* start of first word in comment */
					aToken.start = it;
					aToken.length = 1;
				}
			} else if (ch == ' ' && aToken.length > 0) {
				if (p_cmpToken(&aToken, "post-processed by sm2lbpp") == 0) {
					/* already post-processed file */
					goto onSuccess;
				}
			} else if (ch == ':') {
				/* end of commented parameter key */
				if (aToken.length == 0) {
					aToken.length = (size_t)(it - aToken.start);
				}
				if (p_cmpToken(&aToken, "thumbnail") == 0) {
					/* thumbnail already included */
					goto onSuccess;
				} else if (p_cmpToken(&aToken, "file_total_lines") == 0) {
					totalLinesLine.start = lineStart;
					totalLinesLine.length = 0;
					valueToken = &totalLines;
				} else {
					state = ST_FIND_LINE_START;
				}
				if (valueToken != NULL) {
					memset(&aToken, 0, sizeof(aToken));
					if (valueToken->start == NULL) {
						state = ST_PARAMETER_VALUE;
					} else {
						/* ignore duplicate keys */
						valueToken = NULL;
						state = ST_FIND_LINE_START;
					}
				}
			} else if (isspace(ch) == 0) {
				/* ignore trailing spaces */
				aToken.length = (size_t)(it - aToken.start + 1);
			}
			break;
		case ST_PARAMETER_VALUE:
			if (ch == '\n') {
				/* end of comment line */
				valueToken = NULL;
				state = ST_LINE_START;
				/* remember full line extend to replace it later */
				if (totalLinesLine.start != NULL && totalLinesLine.length == 0) {
					totalLinesLine.length = (size_t)(it - totalLinesLine.start + 1);
				}
			} else if (valueToken->start == NULL) {
				if (isspace(ch) == 0) {
					/* start of comment parameter value */
					valueToken->start = it;
					valueToken->length = 1;
				}
			} else if (isspace(ch) == 0) {
				/* ignore trailing spaces */
				valueToken->length = (size_t)(it - valueToken->start + 1);
			}
			break;
		}
		if (ch == '\n') {
			lineNr++;
			lineStart = it + 1;
		} else if (ch == '\r') {
			lineStart = it + 1;
		}
	}

	if (pointVec != NULL) {
		/* add final path */
		if ((pointVec->size - pointVec->start) > 1) {
			/* move completed path to shape */
			path = pointsToPath(pathPtr, pointVec);
			if (path == NULL) ON_ERROR(MSGT_ERR_NO_MEM);
			pathPtr = &(path->next);
		}

		/* update base address to the most recent value of pointVec->data (there is only one shape) */
		setPointsDataOffset(shape->paths, pointVec->data);
	}

	/* check missing tokens */
	if (totalLines.start == NULL || totalLines.length == 0) ON_WARN(MSGT_WARN_NO_TOTAL_LINES);
	if (totalLinesLine.start == NULL || totalLinesLine.length == 0) ON_WARN(MSGT_WARN_NO_TOTAL_LINES_LINE);

	/* allocate image */
	img = (png_bytep)malloc(IMAGE_WIDTH * IMAGE_HEIGHT * 4);
	if (img == NULL) ON_ERROR(MSGT_ERR_NO_MEM);

	/* there is only one shape with all paths */
	if (shape->paths != NULL) {
		/* realign shape to (5, 5) */
		for (path = shape->paths; path != NULL; path = path->next) {
			for (int i = 0; i < path->npts; i++) {
				path->pts[(2 * i) + 0] -= (minX - BORDER_WIDTH);
				path->pts[(2 * i) + 1] -= (minY - BORDER_HEIGHT);
			}
		}
		/* update shape and path bounds */
		setPathBounds(shape->paths);
		shape->bounds[0] = shape->paths->bounds[0];
		shape->bounds[1] = shape->paths->bounds[1];
		shape->bounds[2] = shape->paths->bounds[2];
		shape->bounds[3] = shape->paths->bounds[3];
		for (path = shape->paths->next; path != NULL; path = path->next) {
			setPathBounds(path);
			shape->bounds[0] = PCF_MIN(shape->bounds[0], path->bounds[0]);
			shape->bounds[1] = PCF_MIN(shape->bounds[1], path->bounds[1]);
			shape->bounds[2] = PCF_MAX(shape->bounds[2], path->bounds[2]);
			shape->bounds[3] = PCF_MAX(shape->bounds[3], path->bounds[3]);
		}
		/* update resolution with border of (5, 5) */
		shape = svg->shapes;
		svg->width = shape->bounds[2] + (2.0f * BORDER_WIDTH);
		svg->height = shape->bounds[3] + (2.0f * BORDER_HEIGHT);
		/* render to image */
		rast = nsvgCreateRasterizer();
		if (rast == NULL) ON_ERROR(MSGT_ERR_NO_MEM);
		const float scaleX = (float)IMAGE_WIDTH / svg->width;
		const float scaleY = (float)IMAGE_HEIGHT / svg->height;
		const float scale = PCF_MIN(scaleX, scaleY);
		/* calculate offset for centered output */
		const float tx = ((float)IMAGE_WIDTH - (svg->width * scale)) / 2.0f;
		const float ty = ((float)IMAGE_HEIGHT - (svg->height * scale)) / 2.0f;
		nsvgRasterize(rast, svg, tx, ty, scale, (unsigned char *)img, IMAGE_WIDTH, IMAGE_HEIGHT, IMAGE_WIDTH * 4);
		/* create opaque background color */
		for (size_t i = 0; i < (IMAGE_WIDTH * IMAGE_HEIGHT); i++) {
			blendByAlpha(img + (4 * i), BACKGROUND_COLOR);
		}
	}

	/* flip vertically and convert bitmap to PNG */
	imgRows = (png_bytepp)malloc(IMAGE_HEIGHT * sizeof(png_bytep));
	if (imgRows == NULL) ON_ERROR(MSGT_ERR_NO_MEM);
	for (size_t i = 0; i < IMAGE_HEIGHT; i++) {
		imgRows[IMAGE_HEIGHT - i - 1] = img + (i * IMAGE_WIDTH * 4);
	}
	switch (imgToPng(imgRows, &png)) {
	case -1:
		ON_ERROR(MSGT_ERR_PNG);
		break;
	case 0:
		ON_ERROR(MSGT_ERR_NO_MEM);
		break;
	default:
		break;
	}

	/* re-create file */
	fp = _tfopen(file, _T("wb"));
	if (fp == NULL) ON_ERROR(MSGT_ERR_FILE_CREATE);

	/* output modified Snapmaker 2.0 specific header */
	clearerr(fp);
	fprintf(fp, ";post-processed by sm2lbpp " PROGRAM_VERSION_STR " (https://github.com/daniel-starke/sm2lbpp)\n");
	/* output until line containing 'file_total_lines' */
	fwrite(inputBuf, (size_t)(totalLinesLine.start - inputBuf), 1, fp);
	/* output corrected line count */
	fprintf(fp, ";file_total_lines: %lu\n", (unsigned long)(lineNr + 2));
	/* output base64 encoded PNG of the preview image */
	fprintf(fp, ";thumbnail: data:image/png;base64,");
	writeBase64(fp, (const unsigned char *)(png.data), png.size);
	fprintf(fp, "\n");
	if (ferror(fp) != 0) ON_ERROR(MSGT_ERR_FILE_WRITE);

	/* output remaining file */
	{
		totalLinesLine.start += totalLinesLine.length;
		if (fwrite(totalLinesLine.start, (size_t)(inputBuf + inputLen - totalLinesLine.start), 1, fp) < 1) {
			ON_ERROR(MSGT_ERR_FILE_WRITE);
		}
	}
onSuccess:
	res = 1;
onError:
	if (png.data != NULL) free(png.data);
	if (imgRows != NULL) free(imgRows);
	if (img != NULL) free(img);
	if (rast != NULL) nsvgDeleteRasterizer(rast);
	if (pointVec != NULL) deletePointVec(pointVec);
	if (svg != NULL) deleteSvg(svg);
	if (fp != NULL) fclose(fp);
	if (inputBuf != NULL) free(inputBuf);
	if (res != 1) {
		_ftprintf(ferr, _T("%s"), fmsg[MSGT_INFO_PRESS_ENTER]);
		_gettchar();
	}
	return res;

#undef ON_WARN
#undef ON_ERROR
}


/**
 * Error output callback for processFile().
 *
 * @param[in] msg - error message ID
 * @param[in] file - input file path
 * @param[in] line - input file path line number (0 if not applicable)
 * @return 1 to continue, 0 to abort file processing
 * @remarks File processing is always aborted on error.
 */
int errorCallback(const tMessage msg, const TCHAR * file, const size_t line) {
	if (line > 0) {
		_ftprintf(ferr, _T("%s:%u: %s"), file, (unsigned)line, fmsg[msg]);
	} else {
		_ftprintf(ferr, _T("%s: %s"), file, fmsg[msg]);
	}
	return 1;
}
