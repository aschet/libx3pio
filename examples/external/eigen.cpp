// Remove the best-fit plane of an x3p file with Eigen.

#include <x3pio/file.h>
#include <x3pio/x3pio.h>

#include <Eigen/Dense>

#include <cmath>
#include <iostream>
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
    x3pio_document_create(nullptr, &document);
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
    x3pio_reader_create(nullptr, &reader);
    return {reader, x3pio_reader_free};
}

Writer create_writer()
{
    x3pio_writer *writer = nullptr;
    x3pio_writer_create(nullptr, &writer);
    return {writer, x3pio_writer_free};
}

// Eigen stores a matrix column by column: the elements of a row are one column
// apart, and the rows are one element apart. The matrix is double, whatever the
// data type of the file, and the library converts the heights.
x3pio_layout layout_of(const Eigen::MatrixXd &matrix)
{
    x3pio_layout layout = *x3pio_layout_packed(X3PIO_TYPE_DOUBLE);
    layout.element_stride =
        static_cast<ptrdiff_t>(matrix.outerStride() * sizeof(double));
    layout.row_stride =
        static_cast<ptrdiff_t>(matrix.innerStride() * sizeof(double));
    return layout;
}

Eigen::MatrixXd read_matrix(x3pio_reader *reader)
{
    const x3pio_document *document = x3pio_reader_document(reader);
    Eigen::MatrixXd heights(
        static_cast<Eigen::Index>(x3pio_document_rows(document)),
        static_cast<Eigen::Index>(x3pio_document_columns(document)));
    const x3pio_layout layout = layout_of(heights);
    check(x3pio_read_heights(reader, &layout, heights.data()),
          x3pio_reader_error_message(reader));
    return heights;
}

// z = offset + slope_x * x + slope_y * y, with x and y counted in points
struct Plane
{
    double offset;
    double slope_x;
    double slope_y;
};

Plane fit_plane(const Eigen::MatrixXd &heights)
{
    // A point that is not measured is NaN and is left out.
    const Eigen::Index count = (heights.array() == heights.array()).count();
    Eigen::MatrixXd design(count, 3);
    Eigen::VectorXd z(count);
    Eigen::Index row = 0;
    const Eigen::Index columns = heights.cols();
    const Eigen::Index rows = heights.rows();
    for (Eigen::Index x = 0; x < columns; ++x)
    {
        const double point_x = static_cast<double>(x);
        for (Eigen::Index y = 0; y < rows; ++y)
        {
            if (!std::isnan(heights(y, x)))
            {
                design.row(row) << 1.0, point_x, static_cast<double>(y);
                z(row++) = heights(y, x);
            }
        }
    }
    const Eigen::Vector3d solution = design.colPivHouseholderQr().solve(z);
    return {solution(0), solution(1), solution(2)};
}

void remove_plane(Eigen::MatrixXd &heights, const Plane &plane)
{
    const Eigen::Index columns = heights.cols();
    const Eigen::Index rows = heights.rows();
    for (Eigen::Index x = 0; x < columns; ++x)
    {
        const double column_offset =
            plane.offset + plane.slope_x * static_cast<double>(x);
        for (Eigen::Index y = 0; y < rows; ++y)
        {
            heights(y, x) -=
                column_offset + plane.slope_y * static_cast<double>(y);
        }
    }
}

void write_matrix(const char *path, const x3pio_document *source,
                  const Eigen::MatrixXd &heights)
{
    Document document = copy_document(source);
    Writer writer = create_writer();
    check(x3pio_create_file(writer.get(), path, document.get()),
          x3pio_writer_error_message(writer.get()));
    const x3pio_layout layout = layout_of(heights);
    check(x3pio_write_heights(writer.get(), &layout, heights.data()),
          x3pio_writer_error_message(writer.get()));
    check(x3pio_end_file(writer.get()),
          x3pio_writer_error_message(writer.get()));
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
        Reader reader = create_reader();
        check(x3pio_open_file(reader.get(), input),
              x3pio_reader_error_message(reader.get()));
        Eigen::MatrixXd heights = read_matrix(reader.get());
        remove_plane(heights, fit_plane(heights));
        write_matrix(output, x3pio_reader_document(reader.get()), heights);
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << "\n";
        return 1;
    }
}
