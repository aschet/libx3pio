// Smooth an x3p file with OpenCV.

#include <x3pio/file.h>
#include <x3pio/x3pio.h>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>

#include <cstddef>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>

namespace
{

using Reader = std::unique_ptr<x3pio_reader, decltype(&x3pio_reader_free)>;
using Writer = std::unique_ptr<x3pio_writer, decltype(&x3pio_writer_free)>;
using Document =
    std::unique_ptr<x3pio_document, decltype(&x3pio_document_free)>;

void check(x3pio_status status, const char *message)
{
    if (status != X3PIO_OK)
    {
        throw std::runtime_error(std::string(x3pio_status_string(status)) + ": "
                                 + message);
    }
}

// The description of the surface as float metres, with the metadata of the
// source.
Document copy_document(const x3pio_document *source)
{
    x3pio_document *document = nullptr;
    x3pio_document_create(&document);
    x3pio_document_copy(document, source);
    x3pio_document_set_surface(document, x3pio_document_columns(source),
                               x3pio_document_rows(source),
                               x3pio_document_axis(source, X3PIO_X).increment,
                               x3pio_document_axis(source, X3PIO_Y).increment,
                               X3PIO_TYPE_FLOAT, 1.0, 1);
    return {document, x3pio_document_free};
}

Reader create_reader()
{
    x3pio_reader *reader = nullptr;
    x3pio_reader_create(&reader);
    return {reader, x3pio_reader_free};
}

Writer create_writer()
{
    x3pio_writer *writer = nullptr;
    x3pio_writer_create(&writer);
    return {writer, x3pio_writer_free};
}

Reader open(const char *path)
{
    Reader reader = create_reader();
    check(x3pio_open_file(reader.get(), path),
          x3pio_reader_error_message(reader.get()));
    return reader;
}

// An image counts its rows from the top and a surface from y = 0. The layout
// has a negative row stride and the pointer given with it is at the last row,
// so the first row of the file is the last row in memory and no copy is
// created. The step of the image is the row stride, so that an image that is
// not continuous, such as a region of interest, is used as it is.
x3pio_layout bottom_up(const cv::Mat &image)
{
    x3pio_layout layout = *x3pio_layout_packed(X3PIO_TYPE_FLOAT);
    layout.row_stride = -static_cast<ptrdiff_t>(image.step);
    return layout;
}

cv::Mat read_heights(x3pio_reader *reader)
{
    const x3pio_document *document = x3pio_reader_document(reader);
    cv::Mat heights(static_cast<int>(x3pio_document_rows(document)),
                    static_cast<int>(x3pio_document_columns(document)), CV_32F);
    const x3pio_layout layout = bottom_up(heights);
    check(x3pio_read_heights(reader, &layout, heights.ptr(heights.rows - 1)),
          x3pio_reader_error_message(reader));
    return heights;
}

// Only the points that are measured enter the average: the blur of the heights
// is divided by the blur of the mask of these points. A point that is not
// measured stays NaN.
cv::Mat smooth(const cv::Mat &heights)
{
    cv::Mat measured;
    cv::compare(heights, heights, measured, cv::CMP_EQ);
    cv::Mat weights;
    measured.convertTo(weights, CV_32F, 1.0 / 255.0);
    cv::Mat values = heights.clone();
    values.setTo(0.0, ~measured);
    cv::GaussianBlur(values, values, cv::Size(5, 5), 1.0);
    cv::GaussianBlur(weights, weights, cv::Size(5, 5), 1.0);
    cv::Mat smoothed;
    cv::divide(values, weights, smoothed);
    smoothed.setTo(std::numeric_limits<float>::quiet_NaN(), ~measured);
    return smoothed;
}

void write_heights(const char *path, const x3pio_document *source,
                   const cv::Mat &heights)
{
    Document document = copy_document(source);
    Writer writer = create_writer();
    check(x3pio_create_file(writer.get(), path, document.get()),
          x3pio_writer_error_message(writer.get()));
    const x3pio_layout layout = bottom_up(heights);
    check(x3pio_write_heights(writer.get(), &layout,
                              heights.ptr(heights.rows - 1)),
          x3pio_writer_error_message(writer.get()));
    check(x3pio_end_file(writer.get()),
          x3pio_writer_error_message(writer.get()));
}

cv::Mat read_into_region(const char *path)
{
    const Reader reader = open(path);
    const x3pio_document *document = x3pio_reader_document(reader.get());
    const int columns = static_cast<int>(x3pio_document_columns(document));
    const int rows = static_cast<int>(x3pio_document_rows(document));
    cv::Mat canvas = cv::Mat::zeros(rows + 100, columns + 200, CV_32F);
    cv::Mat region = canvas(cv::Rect(100, 50, columns, rows));
    const x3pio_layout layout = bottom_up(region);
    check(
        x3pio_read_heights(reader.get(), &layout, region.ptr(region.rows - 1)),
        x3pio_reader_error_message(reader.get()));
    return canvas;
}

} // namespace

int main(int argc, char **argv)
{
    if (argc < 3)
    {
        std::cerr << "usage: " << argv[0] << " input.x3p output.x3p\n";
        return 1;
    }
    const char *input = argv[1];
    const char *output = argv[2];
    try
    {
        const Reader reader = open(input);
        const cv::Mat smoothed = smooth(read_heights(reader.get()));
        write_heights(output, x3pio_reader_document(reader.get()), smoothed);
        read_into_region(input);
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << "\n";
        return 1;
    }
}
