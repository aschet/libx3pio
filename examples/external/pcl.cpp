// Read an x3p file into a Point Cloud Library cloud, thin it out with a voxel
// grid and write the result as a point cloud.

#include <x3pio/file.h>
#include <x3pio/x3pio.h>

#include <pcl/common/common.h>
#include <pcl/filters/voxel_grid.h>
#include <pcl/point_cloud.h>
#include <pcl/point_types.h>

#include <cstddef>
#include <cstdint>
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
using Cloud = pcl::PointCloud<pcl::PointXYZ>;

void check(x3pio_status status, const char *message)
{
    if (status != X3PIO_OK)
    {
        throw std::runtime_error(std::string(x3pio_status_string(status)) + ": "
                                 + message);
    }
}

// The coordinates are the members of the points of the cloud, which a PCL point
// pads to 16 bytes, so the element stride is the size of the point and not that
// of three floats. The library reads and writes the vector of the cloud as it
// is.
constexpr x3pio_layout member_layout{
    X3PIO_TYPE_FLOAT, static_cast<std::ptrdiff_t>(sizeof(pcl::PointXYZ)), 0};

x3pio_fields fields_of(Cloud &cloud)
{
    x3pio_fields fields;
    x3pio_fields_init(&fields);
    pcl::PointXYZ *points = cloud.points.data();
    fields.x = {&points->x, member_layout};
    fields.y = {&points->y, member_layout};
    fields.z = {&points->z, member_layout};
    return fields;
}

x3pio_const_fields fields_of(const Cloud &cloud)
{
    x3pio_const_fields fields;
    x3pio_const_fields_init(&fields);
    const pcl::PointXYZ *points = cloud.points.data();
    fields.x = {&points->x, member_layout};
    fields.y = {&points->y, member_layout};
    fields.z = {&points->z, member_layout};
    return fields;
}

// A surface or a profile is an organized cloud: the matrix of the points, whose
// size is the width and the height. A point that was not measured is NaN, as
// PCL has it. A list of points is an unorganized cloud, which has a height of 1
// and all points in the width. Where a file stores no x and y, the library
// gives the position in the grid times the increments. Of a file with layers,
// the first one is read.
Cloud read_cloud(const char *path)
{
    Reader reader{nullptr, x3pio_reader_free};
    {
        x3pio_reader *created = nullptr;
        check(x3pio_reader_create(&created), "reader");
        reader.reset(created);
    }
    check(x3pio_open_file(reader.get(), path),
          x3pio_reader_error_message(reader.get()));
    const x3pio_document *document = x3pio_reader_document(reader.get());
    const x3pio_dimensions dimensions = x3pio_document_dimensions(document);

    Cloud cloud;
    cloud.width = static_cast<std::uint32_t>(dimensions.size_x);
    cloud.height =
        dimensions.list ? 1U : static_cast<std::uint32_t>(dimensions.size_y);
    cloud.points.resize(static_cast<std::size_t>(cloud.width) * cloud.height);
    cloud.is_dense = false;

    // The rows of the matrix are the rows of the cloud. For a list, a row is a
    // point, and the cloud has the width of the list.
    const x3pio_fields into = fields_of(cloud);
    const std::size_t rows = dimensions.list ? dimensions.size_x : cloud.height;
    check(x3pio_read_points(reader.get(), &into, rows),
          x3pio_reader_error_message(reader.get()));
    return cloud;
}

void write_cloud(const char *path, const Cloud &cloud)
{
    x3pio_document *created = nullptr;
    x3pio_document_create(&created);
    Document document{created, x3pio_document_free};
    x3pio_document_set_point_cloud(document.get(), cloud.points.size(),
                                   X3PIO_TYPE_FLOAT, 1.0);
    x3pio_document_set_software_metadata(document.get(), "pcl", "1.0");

    x3pio_writer *made = nullptr;
    x3pio_writer_create(&made);
    Writer writer{made, x3pio_writer_free};
    check(x3pio_create_file(writer.get(), path, document.get()),
          x3pio_writer_error_message(writer.get()));
    const x3pio_const_fields from = fields_of(cloud);
    check(x3pio_write_points(writer.get(), &from, cloud.points.size()),
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
    try
    {
        const Cloud cloud = read_cloud(argv[1]);
        pcl::PointXYZ lowest;
        pcl::PointXYZ highest;
        pcl::getMinMax3D(cloud, lowest, highest);
        std::cout << cloud.width << " x " << cloud.height << " points, z from "
                  << lowest.z << " m to " << highest.z << " m\n";

        // The filter drops the points that are not finite and thins the rest
        // out to one point per voxel of a thirtieth of the size of the cloud.
        const float leaf = (highest.x - lowest.x) / 30.0F;
        Cloud thinned;
        pcl::VoxelGrid<pcl::PointXYZ> grid;
        grid.setInputCloud(cloud.makeShared());
        grid.setLeafSize(leaf, leaf, leaf);
        grid.filter(thinned);
        std::cout << thinned.size() << " points remain\n";
        write_cloud(argv[2], thinned);
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << "\n";
        return 1;
    }
}
