/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * Status codes, enumerations and the plain structures of the interface.
 */

#ifndef X3PIO_TYPES_H
#define X3PIO_TYPES_H

#include <x3pio/x3pio_export.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <time.h>

#ifdef __cplusplus
extern "C"
{
#endif

/** Result of a function call. The values are part of the interface. */
typedef enum x3pio_status
{
    /** The call succeeded. */
    X3PIO_OK = 0,
    /**
     * An argument is invalid, for example a null pointer, a view of the wrong
     * shape or a document that cannot be written.
     */
    X3PIO_ERR_ARGUMENT = 1,
    /**
     * The call is not valid in the current state of the handle, for example
     * opening a reader a second time.
     */
    X3PIO_ERR_STATE = 2,
    /** A file could not be opened, read or written. */
    X3PIO_ERR_IO = 3,
    /** The data is not an x3p file, is damaged or is truncated. */
    X3PIO_ERR_FORMAT = 4,
    /** The file needs a feature that the library does not have. */
    X3PIO_ERR_UNSUPPORTED = 5,
    /** A number does not fit the type that has to hold it. */
    X3PIO_ERR_RANGE = 6,
    /** A checksum that the file states does not match the data. */
    X3PIO_ERR_CHECKSUM = 7,
    /** An allocation failed. */
    X3PIO_ERR_MEMORY = 8
} x3pio_status;

/**
 * Returns a constant description of a status. The result is never null.
 *
 * @param status The status to describe.
 */
X3PIO_API const char *x3pio_status_string(x3pio_status status);

/** The revision of the standard that a file follows. */
typedef enum x3pio_revision
{
    /**
     * ISO 25178-72:2017, named `ISO5436 - 2000` in the file. It is also the
     * revision of the files of the openGPS library.
     */
    X3PIO_REVISION_2017 = 0,
    /** ISO 25178-72:2017/Amd 1:2020, named `ISO25178-72:2017/DAM1`. */
    X3PIO_REVISION_AMD1 = 1
} x3pio_revision;

/** The class of the 3-D data of a file. */
typedef enum x3pio_feature
{
    /** A linear sequence of points: a matrix of one row. */
    X3PIO_PROFILE = 0,
    /** A matrix of points that have their neighbours. */
    X3PIO_SURFACE = 1,
    /** A list of points without an order. */
    X3PIO_POINT_CLOUD = 2
} x3pio_feature;

/** The three axes of the coordinate system. */
typedef enum x3pio_coordinate
{
    /** The x axis. */
    X3PIO_X = 0,
    /** The y axis. */
    X3PIO_Y = 1,
    /** The z axis. */
    X3PIO_Z = 2
} x3pio_coordinate;

/** How the coordinates along an axis are given. */
typedef enum x3pio_axis_type
{
    /** Not stored: the index of the point times the increment. */
    X3PIO_AXIS_INCREMENTAL = 0,
    /** Stored for every point. */
    X3PIO_AXIS_ABSOLUTE = 1
} x3pio_axis_type;

/**
 * Type of the values of an axis in the file, and of the elements of a buffer
 * of the program.
 *
 * Each enumerator is named after the C type of the values. A file holds the
 * values of an axis as int16_t (`I`), int32_t (`L`), float (`F`) or double
 * (`D`); a buffer can hold any of the eight types, and the library converts.
 * The integers are rounded to the nearest, a tie to the even number.
 */
typedef enum x3pio_type
{
    /** `int8_t`. Not stored in files. */
    X3PIO_TYPE_INT8 = 0,
    /** `uint8_t`. Not stored in files. */
    X3PIO_TYPE_UINT8 = 1,
    /** `int16_t`, `I` in files. */
    X3PIO_TYPE_INT16 = 2,
    /** `uint16_t`. Not stored in files. */
    X3PIO_TYPE_UINT16 = 3,
    /** `int32_t`, `L` in files. */
    X3PIO_TYPE_INT32 = 4,
    /** `uint32_t`. Not stored in files. */
    X3PIO_TYPE_UINT32 = 5,
    /** `float`: IEEE 754 binary32, `F` in files. */
    X3PIO_TYPE_FLOAT = 6,
    /** `double`: IEEE 754 binary64, `D` in files. */
    X3PIO_TYPE_DOUBLE = 7
} x3pio_type;

/**
 * Returns the size in bytes of a value of a type: the size of its C type.
 *
 * @param type The type.
 * @return The size, or 0 if `type` is not an enumerator.
 */
X3PIO_API size_t x3pio_type_size(x3pio_type type);

/**
 * Tells whether a file can store the values of an axis in a type.
 *
 * @param type The type.
 * @return True for int16_t, int32_t, float and double.
 */
X3PIO_API bool x3pio_type_is_stored(x3pio_type type);

/** The kind of probing system of the instrument. */
typedef enum x3pio_probing
{
    /** Not stated. A document needs a type to be written with metadata. */
    X3PIO_PROBING_UNKNOWN = 0,
    /** `Contacting`: a stylus. */
    X3PIO_PROBING_CONTACTING = 1,
    /** `NonContacting`: optical or other. */
    X3PIO_PROBING_NON_CONTACTING = 2,
    /** `Software`: synthetic or filtered data. */
    X3PIO_PROBING_SOFTWARE = 3
} x3pio_probing;

/** How the points of a file are stored. */
typedef enum x3pio_storage
{
    /** In a binary file of the container. Compact and fast. */
    X3PIO_STORAGE_BINARY = 0,
    /** As text in main.xml. Readable, and large. */
    X3PIO_STORAGE_TEXT = 1
} x3pio_storage;

/** How a member of the container is compressed. */
typedef enum x3pio_compression_method
{
    /** Deflate, which every zip program reads. The default. */
    X3PIO_COMPRESS_DEFLATE = 0,
    /** Not compressed: fastest, and right for data that is compressed already.
     */
    X3PIO_COMPRESS_STORE = 1
} x3pio_compression_method;

/**
 * The compression of a member of the container: how, and how hard. A program
 * that does not use an initializer calls x3pio_compression_default().
 */
typedef struct x3pio_compression
{
    /** The method. */
    x3pio_compression_method method;
    /**
     * For deflate, 1 (fastest) to 9 (smallest), or -1 for the default of the
     * compression library. Not used by the method store.
     */
    int level;
} x3pio_compression;

/**
 * Returns the default compression: deflate at the default level.
 */
X3PIO_API x3pio_compression x3pio_compression_default(void);

/**
 * Sets a compression to the default, as x3pio_compression_default() returns it,
 * for a language that cannot take a structure as a return value.
 *
 * @param[out] compression The compression. A null pointer is ignored.
 */
X3PIO_API void x3pio_compression_init(x3pio_compression *compression);

/**
 * Value of the increment of the z axis of a document that requests the writer
 * to select the increment. It is for a file of integers, whose increment trades
 * range against resolution: x3pio_write_heights() determines the smallest
 * increment that fits the heights. A file of floats stores the metres and has
 * the increment 1.
 */
#define X3PIO_AUTO_INCREMENT 0.0

/**
 * One axis of the coordinate system.
 *
 * The structure is frozen for the major version: it has these three fields and
 * a new property of an axis is a new function.
 */
typedef struct x3pio_axis
{
    /** How the coordinates are given. */
    x3pio_axis_type type;
    /**
     * The type of the stored values: int16_t, int32_t, float or double. Not
     * used by an incremental axis.
     */
    x3pio_type storage;
    /**
     * The step of an incremental axis, or the factor that turns the stored
     * values of an absolute axis into metres. Positive and finite, or
     * X3PIO_AUTO_INCREMENT for the z axis of integers.
     */
    double increment;
} x3pio_axis;

/**
 * Where the view coordinate system lies in the global one: a rotation and an
 * offset.
 *
 * With the stored values `s`, the increments `I`, the rotation `R` and the
 * offsets `O`:
 *
 * @code
 * global = R * (I * s) + O
 * @endcode
 *
 * An incremental axis stores no values, and its `s` is the zero-based index of
 * the point along the axis.
 *
 * The structure is frozen for the major version.
 */
typedef struct x3pio_placement
{
    /** The rotation matrix in row-major order: r11 r12 r13 r21 ... r33. */
    double rotation[9];
    /** The offset in metres along x, y and z. */
    double offset[3];
} x3pio_placement;

/**
 * Returns the placement that places nothing: the identity rotation and the
 * offset 0.
 */
X3PIO_API x3pio_placement x3pio_placement_identity(void);

/**
 * Sets a placement to the identity, as x3pio_placement_identity() returns it,
 * for a language that cannot take a structure as a return value.
 *
 * @param[out] placement The placement. A null pointer is ignored.
 */
X3PIO_API void x3pio_placement_init(x3pio_placement *placement);

/**
 * Tells whether the rotation of a placement is not the identity.
 *
 * @param[in] placement The placement.
 */
X3PIO_API bool x3pio_placement_has_rotation(const x3pio_placement *placement);

/**
 * Converts a point from the view to the global coordinate system.
 *
 * @param[in] placement The placement.
 * @param[in] view The coordinates in the view system, in metres.
 * @param[out] global Receives the coordinates in the global system.
 */
X3PIO_API void x3pio_placement_to_global(const x3pio_placement *placement,
                                         const double view[3],
                                         double global[3]);

/**
 * Converts a point from the global to the view coordinate system. The rotation
 * has to be orthonormal, whose inverse is its transpose.
 *
 * @param[in] placement The placement.
 * @param[in] global The coordinates in the global system, in metres.
 * @param[out] view Receives the coordinates in the view system.
 */
X3PIO_API void x3pio_placement_to_view(const x3pio_placement *placement,
                                       const double global[3], double view[3]);

/**
 * The number of points of a file, and how they are arranged.
 *
 * The points are stored with u, the index along x, as the fastest index, then
 * v, then w, the layer. `size_x`, `size_y` and `size_z` are the sizes along u,
 * v and w. A point cloud is a list of `size_x` points.
 *
 * The structure is frozen for the major version.
 */
typedef struct x3pio_dimensions
{
    /** The size along u, the points of a row. */
    size_t size_x;
    /** The size along v, the rows. */
    size_t size_y;
    /** The size along w, the layers. */
    size_t size_z;
    /** A list of points (`ListDimension`), not a matrix. */
    bool list;
} x3pio_dimensions;

/**
 * A date and a time as the ISO 8601 text of the file has them: the reading of
 * the clock, and the offset from UTC if the text names a zone.
 *
 * A file can name a zone or not, and a time_t is an instant and cannot say
 * which. So that a file written again has the text it was read from, the
 * structure keeps both parts. The functions x3pio_timestamp_to_time() and
 * x3pio_timestamp_from_time() convert to and from an instant.
 *
 * The structure is frozen for the major version.
 */
typedef struct x3pio_timestamp
{
    /** The year, 0 to 9999. */
    int year;
    /** The month, 1 to 12. */
    int month;
    /** The day of the month, 1 to 31. */
    int day;
    /** The hour, 0 to 23. */
    int hour;
    /** The minute, 0 to 59. */
    int minute;
    /** The second, 0 to 59. */
    int second;
    /** The microsecond, 0 to 999999. */
    int microsecond;
    /** Whether the text names a zone. */
    bool has_offset;
    /** The offset from UTC in minutes, -840 to 840. Used if `has_offset`. */
    int utc_offset_minutes;
} x3pio_timestamp;

/**
 * Converts a timestamp to the time it stands for, in seconds since
 * 1970-01-01 00:00:00 UTC as POSIX counts them. The microseconds are dropped.
 *
 * @param[in] timestamp The timestamp.
 * @param assumed_offset_minutes The offset to take if the timestamp names no
 * zone.
 * @param[out] utc Receives the time.
 * @retval X3PIO_ERR_ARGUMENT A pointer is null, or a field is out of its range.
 * @retval X3PIO_ERR_RANGE The time does not fit a time_t.
 */
X3PIO_API x3pio_status x3pio_timestamp_to_time(const x3pio_timestamp *timestamp,
                                               int assumed_offset_minutes,
                                               time_t *utc);

/**
 * Converts a time to the timestamp of a zone.
 *
 * @param utc The time.
 * @param offset_minutes The offset of the zone from UTC in minutes.
 * @param[out] timestamp Receives the timestamp, which names the zone.
 * @retval X3PIO_ERR_ARGUMENT `timestamp` is null or the offset is out of range.
 * @retval X3PIO_ERR_RANGE The year of the time is not 0 to 9999.
 */
X3PIO_API x3pio_status x3pio_timestamp_from_time(time_t utc, int offset_minutes,
                                                 x3pio_timestamp *timestamp);

/**
 * Reads the clock of the machine and gives the time in its local zone, as the C
 * library knows the zone.
 *
 * @param[out] timestamp Receives the timestamp, which names the zone.
 * @retval X3PIO_ERR_ARGUMENT `timestamp` is null.
 * @retval X3PIO_ERR_RANGE The clock gives no usable time.
 */
X3PIO_API x3pio_status x3pio_timestamp_now(x3pio_timestamp *timestamp);

/**
 * Formats a timestamp as `YYYY-MM-DDThh:mm:ss.s`, with the shortest fraction of
 * a second of at least one digit and the zone as `+hh:mm` if it has one.
 *
 * @param[in] timestamp The timestamp.
 * @param[out] text Receives the text with a terminator.
 * @param capacity The size of `text`; 40 bytes always suffice.
 * @return The length of the text, which can exceed the capacity, or 0 if the
 * timestamp is invalid.
 */
X3PIO_API size_t x3pio_timestamp_format(const x3pio_timestamp *timestamp,
                                        char *text, size_t capacity);

/**
 * Parses an xsd:dateTime text.
 *
 * @param[in] text The text, in which white space around it is ignored.
 * @param[out] timestamp Receives the timestamp.
 * @return False if the text is not a date and time.
 */
X3PIO_API bool x3pio_timestamp_parse(const char *text,
                                     x3pio_timestamp *timestamp);

#ifdef __cplusplus
}
#endif

#endif /* X3PIO_TYPES_H */
