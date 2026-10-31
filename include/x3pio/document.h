/*
 * SPDX-FileCopyrightText: 2026 Thomas Ascher <thomas.ascher@gmx.at>
 *
 * SPDX-License-Identifier: MIT
 */

/**
 * @file
 * The description of a file: what main.xml says about the points and the
 * measurement.
 *
 * @par Content
 * A document has the revision and the feature type, the three axes, the
 * placement of the view in the global coordinate system, the dimensions of the
 * points, the metadata of the measurement and the identifiers of the vendors
 * whose files the container carries. It does not hold the points. A reader
 * gives the document of a file it has opened, and a writer takes the document
 * of a file it creates.
 */

#ifndef X3PIO_DOCUMENT_H
#define X3PIO_DOCUMENT_H

#include <x3pio/types.h>
#include <x3pio/x3pio_export.h>

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

/**
 * Description of a file, with the names of the elements of main.xml.
 *
 * Setters check what depends on their field alone. x3pio_document_validate()
 * and the creation of a file check the rest, for example that a profile has one
 * row.
 */
typedef struct x3pio_document x3pio_document;

/**
 * Creates a document with default values: the revision of Amd 1, a surface of
 * 0 by 0 points with incremental x and y axes of the increment 1 and an
 * absolute z axis of doubles, the identity placement and no metadata. The
 * caller sets the dimensions, and what differs. Release it with
 * x3pio_document_free().
 *
 * @param[in] allocator The allocator for all memory of the document, or null
 * for x3pio_default_allocator(). The structure is copied, so it need not
 * outlive the call. Both callbacks must be set.
 * @param[out] document Receives the document, or null if the call fails.
 * @retval X3PIO_ERR_ARGUMENT `document` is null, or the allocator lacks a
 * callback.
 * @retval X3PIO_ERR_MEMORY The allocation failed.
 */
X3PIO_API x3pio_status x3pio_document_create(const x3pio_allocator *allocator,
                                             x3pio_document **document);

/**
 * Copies every field of a document into another document. A copy of the
 * document of a reader is one that the caller can change and keep after the
 * reader is freed. The points of a file of text are not part of a document.
 *
 * @param[out] dst The document that receives the fields; it keeps its
 * allocator.
 * @param[in] src The document to copy.
 * @retval X3PIO_ERR_ARGUMENT A pointer is null.
 * @retval X3PIO_ERR_MEMORY An allocation failed. `dst` is unchanged.
 */
X3PIO_API x3pio_status x3pio_document_copy(x3pio_document *dst,
                                           const x3pio_document *src);

/**
 * Releases a document that x3pio_document_create() created. A null pointer is
 * ignored. The document that x3pio_reader_document() returns is not released.
 *
 * @param[in] document The document.
 */
X3PIO_API void x3pio_document_free(x3pio_document *document);

/**
 * Checks that a document can be written as a valid file of its revision. The
 * creation of a file does it too.
 *
 * @param[in] document The document.
 * @param[out] message Receives a constant text that names the first problem,
 * or an empty text. May be null.
 * @retval X3PIO_ERR_ARGUMENT The document is null or cannot be written.
 */
X3PIO_API x3pio_status x3pio_document_validate(const x3pio_document *document,
                                               const char **message);

/**
 * @name Revision and feature type
 * @{
 */

/** Returns the revision. */
X3PIO_API x3pio_revision
x3pio_document_revision(const x3pio_document *document);

/**
 * Sets the revision. The revision of 2017 allows one vendor identifier, which
 * the files of all vendors share.
 *
 * @retval X3PIO_ERR_ARGUMENT A pointer is null, or the value is not an
 * enumerator.
 */
X3PIO_API x3pio_status x3pio_document_set_revision(x3pio_document *document,
                                                   x3pio_revision revision);

/** Returns the feature type. */
X3PIO_API x3pio_feature x3pio_document_feature(const x3pio_document *document);

/**
 * Sets the feature type. A point cloud is a list of points with three absolute
 * axes, a profile or a surface a matrix.
 *
 * @retval X3PIO_ERR_ARGUMENT A pointer is null, or the value is not an
 * enumerator.
 */
X3PIO_API x3pio_status x3pio_document_set_feature(x3pio_document *document,
                                                  x3pio_feature feature);

/** @} */

/**
 * @name Axes, placement and dimensions
 * @{
 */

/**
 * Returns an axis.
 *
 * @param[in] document The document.
 * @param coordinate The axis.
 * @return The axis, or an incremental axis of the increment 1 if the document
 * is null or the coordinate is not an enumerator.
 */
X3PIO_API x3pio_axis x3pio_document_axis(const x3pio_document *document,
                                         x3pio_coordinate coordinate);

/**
 * Returns an axis through a pointer, for a language that cannot take a
 * structure as a return value.
 *
 * @param[in] document The document.
 * @param coordinate The axis.
 * @param[out] axis Receives the axis.
 * @retval X3PIO_ERR_ARGUMENT A pointer is null, or the coordinate is not an
 * enumerator.
 */
X3PIO_API x3pio_status x3pio_document_get_axis(const x3pio_document *document,
                                               x3pio_coordinate coordinate,
                                               x3pio_axis *axis);

/**
 * Sets an axis.
 *
 * @param[in,out] document The document.
 * @param coordinate The axis.
 * @param[in] axis The axis: a positive finite increment and, for an absolute
 * axis, a type that files store.
 * @retval X3PIO_ERR_ARGUMENT A pointer is null, or the axis is invalid.
 */
X3PIO_API x3pio_status x3pio_document_set_axis(x3pio_document *document,
                                               x3pio_coordinate coordinate,
                                               const x3pio_axis *axis);

/** Returns the placement of the view in the global coordinate system. */
X3PIO_API x3pio_placement
x3pio_document_placement(const x3pio_document *document);

/**
 * Returns the placement through a pointer, for a language that cannot take a
 * structure as a return value.
 *
 * @param[in] document The document.
 * @param[out] placement Receives the placement.
 * @retval X3PIO_ERR_ARGUMENT A pointer is null.
 */
X3PIO_API x3pio_status x3pio_document_get_placement(
    const x3pio_document *document, x3pio_placement *placement);

/**
 * Sets the placement.
 *
 * @param[in,out] document The document.
 * @param[in] placement The placement: finite numbers, and a rotation that is
 * orthonormal and does not mirror.
 * @retval X3PIO_ERR_ARGUMENT A pointer is null, or the placement is invalid.
 */
X3PIO_API x3pio_status x3pio_document_set_placement(
    x3pio_document *document, const x3pio_placement *placement);

/** Returns the dimensions of the points. */
X3PIO_API x3pio_dimensions
x3pio_document_dimensions(const x3pio_document *document);

/**
 * Returns the dimensions through a pointer, for a language that cannot take a
 * structure as a return value.
 *
 * @param[in] document The document.
 * @param[out] dimensions Receives the dimensions.
 * @retval X3PIO_ERR_ARGUMENT A pointer is null.
 */
X3PIO_API x3pio_status x3pio_document_get_dimensions(
    const x3pio_document *document, x3pio_dimensions *dimensions);

/**
 * Sets the dimensions to a matrix of `size_x` by `size_y` points in `size_z`
 * layers.
 *
 * @retval X3PIO_ERR_ARGUMENT `document` is null.
 */
X3PIO_API x3pio_status x3pio_document_set_matrix(x3pio_document *document,
                                                 size_t size_x, size_t size_y,
                                                 size_t size_z);

/**
 * Sets the dimensions to a list of points.
 *
 * @retval X3PIO_ERR_ARGUMENT `document` is null.
 */
X3PIO_API x3pio_status x3pio_document_set_point_list(x3pio_document *document,
                                                     size_t count);

/**
 * Returns the number of points: the product of the dimensions, or 0 if it does
 * not fit a size_t.
 */
X3PIO_API size_t x3pio_document_point_count(const x3pio_document *document);

/**
 * Returns the number of columns of the matrix that the functions transfer: the
 * points of a row, `size_x`, and 1 for a list of points, which is a single
 * column.
 */
X3PIO_API size_t x3pio_document_columns(const x3pio_document *document);

/**
 * Returns the number of rows of that matrix: `size_y` times `size_z`, the rows
 * of the first layer, then those of the second layer, and so on; and the number
 * of points for a list of points, which can be read and written in any number
 * of parts. It is 0 if the product does not fit a size_t.
 */
X3PIO_API size_t x3pio_document_rows(const x3pio_document *document);

/**
 * Tells whether a file carries a mask of the points that were measured: a file
 * of validity bits, or in a file of text the points without a value. The
 * document of a program has none. Floats mark the points that were not measured
 * in the values themselves, whereas integers need a mask.
 */
X3PIO_API bool x3pio_document_has_validity(const x3pio_document *document);

/** @} */

/**
 * @name Ready-made descriptions
 *
 * Each function resets the feature type, the axes and the dimensions, and
 * keeps the rest, the metadata and the placement.
 * @{
 */

/**
 * Describes a surface on a regular grid: `columns` points along x and `rows`
 * along y, `x_step` and `y_step` metres apart, in `layers` layers. Only the
 * heights are stored, as `z_type`. A float type stores the metres, with the z
 * increment 1. An integer type needs an increment in metres that fits the
 * heights, or X3PIO_AUTO_INCREMENT, which x3pio_write_heights() resolves, see
 * x3pio_suggest_increment_for_range().
 *
 * @retval X3PIO_ERR_ARGUMENT `document` is null, or a number is invalid.
 */
X3PIO_API x3pio_status x3pio_document_set_surface(
    x3pio_document *document, size_t columns, size_t rows, double x_step,
    double y_step, x3pio_type z_type, double z_increment, size_t layers);

/**
 * Describes a profile of `points` points `x_step` metres apart, in `layers`
 * layers.
 *
 * @retval X3PIO_ERR_ARGUMENT `document` is null, or a number is invalid.
 */
X3PIO_API x3pio_status x3pio_document_set_profile(x3pio_document *document,
                                                  size_t points, double x_step,
                                                  x3pio_type z_type,
                                                  double z_increment,
                                                  size_t layers);

/**
 * Describes a point cloud of `count` points whose three coordinates are stored
 * as `type`.
 *
 * @retval X3PIO_ERR_ARGUMENT `document` is null, or a number is invalid.
 */
X3PIO_API x3pio_status x3pio_document_set_point_cloud(x3pio_document *document,
                                                      size_t count,
                                                      x3pio_type type,
                                                      double increment);

/**
 * Describes an irregular grid: a surface whose points are free in x, y and z,
 * stored as `type`, with the points in a matrix.
 *
 * @retval X3PIO_ERR_ARGUMENT `document` is null, or a number is invalid.
 */
X3PIO_API x3pio_status x3pio_document_set_irregular_surface(
    x3pio_document *document, size_t columns, size_t rows, x3pio_type type,
    double increment, size_t layers);

/** @} */

/**
 * @name Metadata
 *
 * The record of the measurement. It is optional. Every setter makes the
 * document carry it, and a file that has it needs the date and the type of the
 * probing system. The strings are UTF-8, and a getter returns an empty string
 * for a text that is not set. The pointer of a getter is valid until the text
 * is set again or the document is freed.
 * @{
 */

/** Tells whether the document has metadata. */
X3PIO_API bool x3pio_document_has_metadata(const x3pio_document *document);

/**
 * Removes the metadata.
 *
 * @param[in,out] document The document.
 */
X3PIO_API void x3pio_document_clear_metadata(x3pio_document *document);

/**
 * Sets the metadata of data that software made, for example a filter or a
 * simulation: made now, with the software as the instrument and as the probing
 * system. The previous metadata is replaced.
 *
 * @param[in,out] document The document.
 * @param[in] name The name of the software.
 * @param[in] version The version of the software.
 * @retval X3PIO_ERR_ARGUMENT A pointer is null.
 * @retval X3PIO_ERR_RANGE The clock gives no usable time.
 * @retval X3PIO_ERR_MEMORY An allocation failed.
 */
X3PIO_API x3pio_status x3pio_document_set_software_metadata(
    x3pio_document *document, const char *name, const char *version);

/**
 * Gets the date of the data set.
 *
 * @param[in] document The document.
 * @param[out] date Receives the date. May be null.
 * @return False if the document has none.
 */
X3PIO_API bool x3pio_document_date(const x3pio_document *document,
                                   x3pio_timestamp *date);

/**
 * Sets the date of the data set, which a file with metadata requires.
 *
 * @param[in,out] document The document.
 * @param[in] date The date, or null to remove it.
 * @retval X3PIO_ERR_ARGUMENT `document` is null, or the date is invalid.
 */
X3PIO_API x3pio_status x3pio_document_set_date(x3pio_document *document,
                                               const x3pio_timestamp *date);

/**
 * Gets the date of the last calibration of the instrument.
 *
 * @param[in] document The document.
 * @param[out] date Receives the date. May be null.
 * @return False if the document has none.
 */
X3PIO_API bool x3pio_document_calibration_date(const x3pio_document *document,
                                               x3pio_timestamp *date);

/**
 * Sets the date of the last calibration of the instrument.
 *
 * @param[in,out] document The document.
 * @param[in] date The date, or null to remove it.
 * @retval X3PIO_ERR_ARGUMENT `document` is null, or the date is invalid.
 */
X3PIO_API x3pio_status x3pio_document_set_calibration_date(
    x3pio_document *document, const x3pio_timestamp *date);

/** Returns who made the data set. */
X3PIO_API const char *x3pio_document_creator(const x3pio_document *document);

/**
 * Sets who made the data set.
 *
 * @retval X3PIO_ERR_ARGUMENT A pointer is null.
 * @retval X3PIO_ERR_MEMORY An allocation failed.
 */
X3PIO_API x3pio_status x3pio_document_set_creator(x3pio_document *document,
                                                  const char *creator);

/** Returns the manufacturer of the instrument. */
X3PIO_API const char *
x3pio_document_manufacturer(const x3pio_document *document);

/** Sets the manufacturer of the instrument, as x3pio_document_set_creator(). */
X3PIO_API x3pio_status x3pio_document_set_manufacturer(
    x3pio_document *document, const char *manufacturer);

/** Returns the model of the instrument. */
X3PIO_API const char *x3pio_document_model(const x3pio_document *document);

/** Sets the model of the instrument, as x3pio_document_set_creator(). */
X3PIO_API x3pio_status x3pio_document_set_model(x3pio_document *document,
                                                const char *model);

/** Returns the serial number of the instrument. */
X3PIO_API const char *x3pio_document_serial(const x3pio_document *document);

/** Sets the serial number of the instrument, as x3pio_document_set_creator().
 */
X3PIO_API x3pio_status x3pio_document_set_serial(x3pio_document *document,
                                                 const char *serial);

/** Returns the version of the instrument or its software. */
X3PIO_API const char *x3pio_document_version(const x3pio_document *document);

/** Sets the version of the instrument, as x3pio_document_set_creator(). */
X3PIO_API x3pio_status x3pio_document_set_version(x3pio_document *document,
                                                  const char *version);

/** Returns the kind of the probing system. */
X3PIO_API x3pio_probing
x3pio_document_probing_type(const x3pio_document *document);

/**
 * Sets the kind of the probing system, which a file with metadata requires.
 *
 * @retval X3PIO_ERR_ARGUMENT `document` is null, or the value is not an
 * enumerator.
 */
X3PIO_API x3pio_status x3pio_document_set_probing_type(x3pio_document *document,
                                                       x3pio_probing type);

/** Returns the description of the probing system. */
X3PIO_API const char *
x3pio_document_probing_identification(const x3pio_document *document);

/**
 * Sets the description of the probing system, as x3pio_document_set_creator().
 */
X3PIO_API x3pio_status x3pio_document_set_probing_identification(
    x3pio_document *document, const char *identification);

/** Returns the comment. */
X3PIO_API const char *x3pio_document_comment(const x3pio_document *document);

/** Sets the comment, as x3pio_document_set_creator(). */
X3PIO_API x3pio_status x3pio_document_set_comment(x3pio_document *document,
                                                  const char *comment);

/** @} */

/**
 * @name Vendors
 *
 * The identifiers of the vendors whose files the container carries. A reader
 * reports them. A writer adds them with x3pio_add_vendor_file().
 * @{
 */

/** Returns the number of vendor identifiers. */
X3PIO_API size_t x3pio_document_vendor_count(const x3pio_document *document);

/**
 * Returns a vendor identifier.
 *
 * @param[in] document The document.
 * @param index A number below x3pio_document_vendor_count().
 * @return The identifier, or null if the index is not below the count.
 */
X3PIO_API const char *x3pio_document_vendor_id(const x3pio_document *document,
                                               size_t index);

/** @} */

#ifdef __cplusplus
}
#endif

#endif /* X3PIO_DOCUMENT_H */
