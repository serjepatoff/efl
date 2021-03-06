#ifndef EMILE_IMAGE_H
#define EMILE_IMAGE_H

/**
 * @defgroup Emile_Image_Group Top level functions
 * @ingroup Emile
 * Function that allow reading/saving image.
 *
 * @{
 */

typedef Efl_Gfx_Colorspace Emile_Colorspace;

#define EMILE_COLORSPACE_ARGB8888 EFL_GFX_COLORSPACE_ARGB8888
#define EMILE_COLORSPACE_YCBCR422P601_PL EFL_GFX_COLORSPACE_YCBCR422P601_PL
#define EMILE_COLORSPACE_YCBCR422P709_PL EFL_GFX_COLORSPACE_YCBCR422P709_PL
#define EMILE_COLORSPACE_RGB565_A5P EFL_GFX_COLORSPACE_RGB565_A5P
#define EMILE_COLORSPACE_GRY8 EFL_GFX_COLORSPACE_GRY8
#define EMILE_COLORSPACE_YCBCR422601_PL EFL_GFX_COLORSPACE_YCBCR422601_PL
#define EMILE_COLORSPACE_YCBCR420NV12601_PL EFL_GFX_COLORSPACE_YCBCR420NV12601_PL
#define EMILE_COLORSPACE_YCBCR420TM12601_PL EFL_GFX_COLORSPACE_YCBCR420TM12601_PL
#define EMILE_COLORSPACE_AGRY88 EFL_GFX_COLORSPACE_AGRY88
   // ETC1/2 support
#define EMILE_COLORSPACE_ETC1 EFL_GFX_COLORSPACE_ETC1
#define EMILE_COLORSPACE_RGB8_ETC2 EFL_GFX_COLORSPACE_RGB8_ETC2
#define EMILE_COLORSPACE_RGBA8_ETC2_EAC EFL_GFX_COLORSPACE_RGBA8_ETC2_EAC
#define EMILE_COLORSPACE_ETC1_ALPHA EFL_GFX_COLORSPACE_ETC1_ALPHA
   // S3TC support
#define EMILE_COLORSPACE_RGB_S3TC_DXT1 EFL_GFX_COLORSPACE_RGB_S3TC_DXT1
#define EMILE_COLORSPACE_RGBA_S3TC_DXT1 EFL_GFX_COLORSPACE_RGBA_S3TC_DXT1
#define EMILE_COLORSPACE_RGBA_S3TC_DXT2 EFL_GFX_COLORSPACE_RGBA_S3TC_DXT2
#define EMILE_COLORSPACE_RGBA_S3TC_DXT3 EFL_GFX_COLORSPACE_RGBA_S3TC_DXT3
#define EMILE_COLORSPACE_RGBA_S3TC_DXT4 EFL_GFX_COLORSPACE_RGBA_S3TC_DXT4
#define EMILE_COLORSPACE_RGBA_S3TC_DXT5 EFL_GFX_COLORSPACE_RGBA_S3TC_DXT5

/**
 * @typedef Emile_Image_Encoding
 *
 * Flags that describe the supported encoding. Some routine may not know all of them.
 * The value are the same as the one provided before in Eet.h
 *
 * @see Eet_Image_Encoding
 *
 * @since 1.14
 */
typedef enum _Emile_Image_Encoding
{
  EMILE_IMAGE_LOSSLESS = 0,
  EMILE_IMAGE_JPEG = 1,
  EMILE_IMAGE_ETC1 = 2,
  EMILE_IMAGE_ETC2_RGB = 3,
  EMILE_IMAGE_ETC2_RGBA = 4,
  EMILE_IMAGE_ETC1_ALPHA = 5
} Emile_Image_Encoding;

/**
 * @typedef Emile_Image_Scale_Hint
 *
 * Flags that describe the scale hint used by the loader infrastructure.
 *
 * @see Evas_Image_Scale_Hint
 *
 * @since 1.14
 */
typedef enum _Emile_Image_Scale_Hint
{
  EMILE_IMAGE_SCALE_HINT_NONE = 0, /**< No scale hint at all */
  EMILE_IMAGE_SCALE_HINT_DYNAMIC = 1, /**< Image is being re-scaled over time, thus turning scaling cache @b off for its data */
  EMILE_IMAGE_SCALE_HINT_STATIC = 2 /**< Image is not being re-scaled over time, thus turning scaling cache @b on for its data */
} Emile_Image_Scale_Hint;

/**
 * @typedef Emile_Image_Animated_Loop_Hint
 *
 * Flags describing the behavior of animation from a loaded image.
 *
 * @see Evas_Image_Animated_Loop_Hint
 *
 * @since 1.14
 */
typedef enum _Emile_Image_Animated_Loop_Hint
{
  EMILE_IMAGE_ANIMATED_HINT_NONE = 0,
  EMILE_IMAGE_ANIMATED_HINT_LOOP = 1,
  EMILE_IMAGE_ANIMATED_HINT_PINGPONG = 2
} Emile_Image_Animated_Loop_Hint;

/**
 * @typedef Emile_Image_Load_Error
 *
 * Flags describing error state as discovered by an image loader.
 *
 * @see Evas_Load_Error
 *
 * @since 1.14
 */
typedef enum _Emile_Image_Load_Error
{
  EMILE_IMAGE_LOAD_ERROR_NONE = 0,  /**< No error on load */
  EMILE_IMAGE_LOAD_ERROR_GENERIC = 1,  /**< A non-specific error occurred */
  EMILE_IMAGE_LOAD_ERROR_DOES_NOT_EXIST = 2,  /**< File (or file path) does not exist */
  EMILE_IMAGE_LOAD_ERROR_PERMISSION_DENIED = 3,	 /**< Permission denied to an existing file (or path) */
  EMILE_IMAGE_LOAD_ERROR_RESOURCE_ALLOCATION_FAILED = 4,  /**< Allocation of resources failure prevented load */
  EMILE_IMAGE_LOAD_ERROR_CORRUPT_FILE = 5,  /**< File corrupt (but was detected as a known format) */
  EMILE_IMAGE_LOAD_ERROR_UNKNOWN_FORMAT = 6  /**< File is not a known format */
} Emile_Image_Load_Error; /**< Emile image load error codes one can get - see emile_load_error_str() too. */

/**
 * @typedef Emile_Image
 *
 * Internal type representing an opened image.
 *
 * @since 1.14
 */
typedef struct _Emile_Image Emile_Image;

/**
 * @typedef Emile_Image_Load_Opts
 *
 * Description of the possible load option.
 *
 * @since 1.14
 */
typedef struct _Emile_Image_Load_Opts Emile_Image_Load_Opts;

/**
 * @typedef Emile_Image_Animated
 *
 * Description animation.
 *
 * @since 1.14
 */
typedef struct _Emile_Image_Animated Emile_Image_Animated;

/**
 * @typedef Emile_Image_Property
 *
 * Description of a loaded image property.
 *
 * @since 1.14
 */
typedef struct _Emile_Image_Property Emile_Image_Property;

struct _Emile_Image_Property
{
  struct
  {
    unsigned char l, r, t, b;
  } borders;

  const Emile_Colorspace *cspaces;
  Emile_Colorspace cspace;

  Emile_Image_Encoding encoding;

  unsigned int w;
  unsigned int h;
  unsigned int row_stride;

  unsigned char scale;

  Eina_Bool rotated;
  Eina_Bool alpha;
  Eina_Bool premul;
  Eina_Bool alpha_sparse;

  Eina_Bool flipped;
  Eina_Bool comp;
};

struct _Emile_Image_Animated
{
  Eina_List *frames;

  Emile_Image_Animated_Loop_Hint loop_hint;

  int frame_count;
  int loop_count;
  int cur_frame;

  Eina_Bool animated;
};

struct _Emile_Image_Load_Opts
{
  Eina_Rectangle region;
  struct
  {
    int src_x, src_y, src_w, src_h;
    int dst_w, dst_h;
    int smooth;

    /* This should have never been part of this structure, but we keep it
       for ABI/API compability with Evas_Loader */
    Emile_Image_Scale_Hint scale_hint;
  } scale_load;
  double dpi;
  unsigned int w, h;
  unsigned int degree;
  int scale_down_by;

  Eina_Bool orientation;
};

// FIXME: should we set region at load time, instead of head time
// FIXME: should we regive the animated structure for head and data ?

/**
 * Open a TGV image from memory.
 *
 * @param source The Eina_Binbuf with TGV image in it.
 * @param opts Load option for the image to open (it can be @c NULL).
 * @param animated Description of the image animation property, set during head reading and updated for each frame read by data (can be @c NULL)
 * @param error Contain a valid error code if the function return @c NULL.
 * @return a handler of the image if successfully opened, otherwise @c NULL.
 *
 * @since 1.14
 */
EAPI Emile_Image *emile_image_tgv_memory_open(Eina_Binbuf * source, Emile_Image_Load_Opts * opts, Emile_Image_Animated * animated, Emile_Image_Load_Error * error);

/**
 * Open a TGV image from a file.
 *
 * @param source The Eina_File with TGV image in it.
 * @param opts Load option for the image to open (it can be @c NULL).
 * @param animated Description of the image animation property, set during head reading and updated for each frame read by data (can be @c NULL)
 * @param error Contain a valid error code if the function return @c NULL.
 * @return a handler of the image if successfully opened, otherwise @c NULL.
 *
 * @since 1.14
 */
EAPI Emile_Image *emile_image_tgv_file_open(Eina_File * source, Emile_Image_Load_Opts * opts, Emile_Image_Animated * animated, Emile_Image_Load_Error * error);


/**
 * Open a JPEG image from memory.
 *
 * @param source The Eina_Binbuf with JPEG image in it.
 * @param opts Load option for the image to open (it can be @c NULL).
 * @param animated Description of the image animation property, set during head reading and updated for each frame read by data (can be @c NULL)
 * @param error Contain a valid error code if the function return @c NULL.
 * @return a handler of the image if successfully opened, otherwise @c NULL.
 *
 * @since 1.14
 */
EAPI Emile_Image *emile_image_jpeg_memory_open(Eina_Binbuf * source, Emile_Image_Load_Opts * opts, Emile_Image_Animated * animated, Emile_Image_Load_Error * error);

/**
 * Open a JPEG image from file.
 *
 * @param source The Eina_File with JPEG image in it.
 * @param opts Load option for the image to open (it can be @c NULL).
 * @param animated Description of the image animation property, set during head reading and updated for each frame read by data (can be @c NULL)
 * @param error Contain a valid error code if the function return @c NULL.
 * @return a handler of the image if successfully opened, otherwise @c NULL.
 *
 * @since 1.14
 */
EAPI Emile_Image *emile_image_jpeg_file_open(Eina_File * source, Emile_Image_Load_Opts * opts, Emile_Image_Animated * animated, Emile_Image_Load_Error * error);

/**
 * Read the header of an image to fill Emile_Image_Property.
 *
 * @param image The Emile_Image handler.
 * @param prop The Emile_Image_Property to be filled.
 * @param property_size The size of the Emile_Image_Property as known during compilation.
 * @param error Contain a valid error code if the function return @c NULL.
 * @return @c EINA_TRUE if the header was successfully readed and prop properly filled.
 *
 * @since 1.14
 */
EAPI Eina_Bool emile_image_head(Emile_Image * image, Emile_Image_Property * prop, unsigned int property_size, Emile_Image_Load_Error * error);

/**
 * Read the pixels from an image file.
 *
 * @param image The Emile_Image handler.
 * @param prop The property to respect while reading this pixels.
 * @param property_size The size of the Emile_Image_Property as known during compilation.
 * @param pixels The actual pointer to the already allocated pixels buffer to fill.
 * @param error Contain a valid error code if the function return @c NULL.
 * @return @c EINA_TRUE if the data was successfully read and the pixels correctly filled.
 *
 * @since 1.14
 */
EAPI Eina_Bool emile_image_data(Emile_Image * image, Emile_Image_Property * prop, unsigned int property_size, void *pixels, Emile_Image_Load_Error * error);

/**
 * Close an opened image handler.
 *
 * @param source The handler to close.
 *
 * @since 1.14
 */
EAPI void emile_image_close(Emile_Image * source);

/**
 * Convert an error code related to an image handler into a meaningful string.
 *
 * @param source The handler related to the error (can be @c NULL).
 * @param error The error code to get a message from.
 * @return a string that will be owned by Emile, either by the handler if it is not @c NULL or by the library directly if it is.
 *
 * @since 1.14
 */
EAPI const char *emile_load_error_str(Emile_Image * source, Emile_Image_Load_Error error);

/**
 * @}
 */

#endif
