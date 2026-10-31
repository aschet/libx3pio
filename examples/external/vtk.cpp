// Display an x3p file with VTK: a profile, a regular or an irregular surface,
// each layer of them, or a point cloud.

#include <x3pio/file.h>
#include <x3pio/x3pio.h>

#include <vtkActor.h>
#include <vtkCamera.h>
#include <vtkCellArray.h>
#include <vtkDataSet.h>
#include <vtkDataSetAttributes.h>
#include <vtkDataSetMapper.h>
#include <vtkFloatArray.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkLookupTable.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPoints.h>
#include <vtkPolyData.h>
#include <vtkProperty.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>
#include <vtkStructuredGrid.h>
#include <vtkUnsignedCharArray.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

using Reader = std::unique_ptr<x3pio_reader, decltype(&x3pio_reader_free)>;

void check(x3pio_status status, const char *message)
{
    if (status != X3PIO_OK)
    {
        throw std::runtime_error(std::string(x3pio_status_string(status)) + ": "
                                 + message);
    }
}

Reader create_reader()
{
    x3pio_reader *reader = nullptr;
    x3pio_reader_create(nullptr, &reader);
    return {reader, x3pio_reader_free};
}

// A point of the program. VTK stores the coordinates of its points as three
// floats that follow each other, which is the layout of this structure, so the
// vertices are handed to VTK as they are.
struct Vertex
{
    float x;
    float y;
    float z;
};
static_assert(sizeof(Vertex) == 3 * sizeof(float), "VTK needs no padding");

// What a file holds: the shape, the vertices of all layers one after the other,
// and for each vertex whether it was not measured.
struct Data
{
    x3pio_feature feature;
    size_t columns;
    size_t rows_per_layer;
    size_t layers;
    std::vector<Vertex> vertices;
    std::vector<unsigned char> hidden;
};

// One call reads every kind of file. The three coordinates are bound to the
// members of the vertices: the element stride is the size of the structure.
// Where the file stores no x and y, as a regular grid does not, the library
// gives the position of the point in the grid times the increments. A point
// that was not measured is NaN in each coordinate that the file stores, and a
// file with an irregular grid or a cloud stores all three. The file can be read
// in pieces of rows, and a list of points has a row for each point.
Data read_data(x3pio_reader *reader)
{
    const x3pio_document *document = x3pio_reader_document(reader);
    const x3pio_dimensions dimensions = x3pio_document_dimensions(document);
    Data data{x3pio_document_feature(document),
              x3pio_document_columns(document),
              dimensions.list ? 1 : dimensions.size_y,
              dimensions.list ? 1 : dimensions.size_z,
              {},
              {}};
    const size_t rows = x3pio_document_rows(document);
    data.vertices.resize(x3pio_document_point_count(document));

    const auto stride = static_cast<ptrdiff_t>(sizeof(Vertex));
    Vertex *first = data.vertices.data();
    x3pio_fields into;
    x3pio_fields_init(&into);
    into.x = {&first->x, {X3PIO_TYPE_FLOAT, stride, 0}};
    into.y = {&first->y, {X3PIO_TYPE_FLOAT, stride, 0}};
    into.z = {&first->z, {X3PIO_TYPE_FLOAT, stride, 0}};
    check(x3pio_read_points(reader, &into, rows),
          x3pio_reader_error_message(reader));

    // VTK cannot take a NaN in its bounds: the points not measured are marked
    // and moved to the origin.
    data.hidden.assign(data.vertices.size(), 0);
    for (size_t i = 0; i < data.vertices.size(); ++i)
    {
        Vertex &vertex = data.vertices[i];
        if (!std::isfinite(vertex.x) || !std::isfinite(vertex.y)
            || !std::isfinite(vertex.z))
        {
            data.hidden[i] = vtkDataSetAttributes::HIDDENPOINT;
            vertex = {0.0F, 0.0F, 0.0F};
        }
    }
    return data;
}

// The coordinates of vertices[first, first + count) as the points of VTK,
// without a copy: the array of VTK uses the memory of the vector.
vtkSmartPointer<vtkPoints> points_of(Data &data, size_t first, size_t count)
{
    vtkNew<vtkFloatArray> coordinates;
    coordinates->SetNumberOfComponents(3);
    coordinates->SetArray(&data.vertices[first].x,
                          static_cast<vtkIdType>(3 * count), 1);
    auto points = vtkSmartPointer<vtkPoints>::New();
    points->SetData(coordinates);
    return points;
}

// The heights colour the points, and the points not measured are hidden.
void add_attributes(Data &data, vtkDataSet *set, size_t first, size_t count)
{
    vtkNew<vtkFloatArray> heights;
    heights->SetName("z");
    heights->SetNumberOfValues(static_cast<vtkIdType>(count));
    vtkNew<vtkUnsignedCharArray> ghosts;
    ghosts->SetName(vtkDataSetAttributes::GhostArrayName());
    ghosts->SetNumberOfValues(static_cast<vtkIdType>(count));
    for (size_t i = 0; i < count; ++i)
    {
        heights->SetValue(static_cast<vtkIdType>(i),
                          data.vertices[first + i].z);
        ghosts->SetValue(static_cast<vtkIdType>(i), data.hidden[first + i]);
    }
    set->GetPointData()->SetScalars(heights);
    set->GetPointData()->AddArray(ghosts);
}

// A surface is a grid of points that have their neighbours, whether the grid
// is regular or its points are free in x, y and z.
vtkSmartPointer<vtkDataSet> grid_of_layer(Data &data, size_t layer)
{
    const size_t count = data.columns * data.rows_per_layer;
    auto grid = vtkSmartPointer<vtkStructuredGrid>::New();
    grid->SetDimensions(static_cast<int>(data.columns),
                        static_cast<int>(data.rows_per_layer), 1);
    grid->SetPoints(points_of(data, layer * count, count));
    add_attributes(data, grid, layer * count, count);
    return grid;
}

// A profile of a layer is a line through the points that were measured, broken
// where a point is missing.
vtkSmartPointer<vtkDataSet> line_of_layer(Data &data, size_t layer)
{
    const size_t count = data.columns * data.rows_per_layer;
    const size_t first = layer * count;
    auto line = vtkSmartPointer<vtkPolyData>::New();
    line->SetPoints(points_of(data, first, count));
    vtkNew<vtkCellArray> segments;
    for (size_t row = 0; row < data.rows_per_layer; ++row)
    {
        const size_t start = first + row * data.columns;
        for (size_t i = 1; i < data.columns; ++i)
        {
            if (!data.hidden[start + i - 1] && !data.hidden[start + i])
            {
                const vtkIdType ends[2] = {
                    static_cast<vtkIdType>(row * data.columns + i - 1),
                    static_cast<vtkIdType>(row * data.columns + i)};
                segments->InsertNextCell(2, ends);
            }
        }
    }
    line->SetLines(segments);
    add_attributes(data, line, first, count);
    return line;
}

// A point cloud has no neighbours: each point that was measured is a vertex.
vtkSmartPointer<vtkDataSet> cloud_of(Data &data)
{
    const size_t count = data.vertices.size();
    auto cloud = vtkSmartPointer<vtkPolyData>::New();
    cloud->SetPoints(points_of(data, 0, count));
    vtkNew<vtkCellArray> vertices;
    for (size_t i = 0; i < count; ++i)
    {
        if (!data.hidden[i])
        {
            const auto id = static_cast<vtkIdType>(i);
            vertices->InsertNextCell(1, &id);
        }
    }
    cloud->SetVerts(vertices);
    add_attributes(data, cloud, 0, count);
    return cloud;
}

std::vector<vtkSmartPointer<vtkDataSet>> sets_of(Data &data)
{
    std::vector<vtkSmartPointer<vtkDataSet>> sets;
    if (data.feature == X3PIO_POINT_CLOUD)
    {
        sets.push_back(cloud_of(data));
        return sets;
    }
    for (size_t layer = 0; layer < data.layers; ++layer)
    {
        sets.push_back(data.feature == X3PIO_PROFILE
                           ? line_of_layer(data, layer)
                           : grid_of_layer(data, layer));
    }
    return sets;
}

struct Extent
{
    std::array<double, 2> x{std::numeric_limits<double>::max(),
                            std::numeric_limits<double>::lowest()};
    std::array<double, 2> y = x;
    std::array<double, 2> z = x;
};

Extent extent_of(const Data &data)
{
    Extent extent;
    for (size_t i = 0; i < data.vertices.size(); ++i)
    {
        if (!data.hidden[i])
        {
            const Vertex &v = data.vertices[i];
            extent.x = {std::min<double>(extent.x[0], v.x),
                        std::max<double>(extent.x[1], v.x)};
            extent.y = {std::min<double>(extent.y[0], v.y),
                        std::max<double>(extent.y[1], v.y)};
            extent.z = {std::min<double>(extent.z[0], v.z),
                        std::max<double>(extent.z[1], v.z)};
        }
    }
    return extent;
}

// The relief is raised to a quarter of the width of the object, as a few
// micrometres of height would not be seen on a surface of millimetres.
void view(Data &data)
{
    const Extent extent = extent_of(data);
    const double width =
        std::max(extent.x[1] - extent.x[0], extent.y[1] - extent.y[0]);
    const double relief = extent.z[1] - extent.z[0];
    const double raise =
        relief > 0.0 && width > 0.0 ? 0.25 * width / relief : 1.0;

    vtkNew<vtkLookupTable> colours;
    colours->SetHueRange(0.667, 0.0);
    colours->Build();
    vtkNew<vtkRenderer> renderer;
    for (const auto &set : sets_of(data))
    {
        vtkNew<vtkDataSetMapper> mapper;
        mapper->SetInputData(set);
        mapper->SetLookupTable(colours);
        mapper->SetScalarRange(extent.z[0], extent.z[1]);
        vtkNew<vtkActor> actor;
        actor->SetMapper(mapper);
        actor->SetScale(1.0, 1.0, raise);
        actor->GetProperty()->SetPointSize(3.0F);
        actor->GetProperty()->SetLineWidth(2.0F);
        renderer->AddActor(actor);
    }
    renderer->SetBackground(0.1, 0.1, 0.15);
    renderer->GetActiveCamera()->Elevation(-40.0);
    renderer->GetActiveCamera()->OrthogonalizeViewUp();
    renderer->ResetCamera();
    vtkNew<vtkRenderWindow> window;
    window->AddRenderer(renderer);
    window->SetSize(800, 600);
    vtkNew<vtkRenderWindowInteractor> interactor;
    vtkNew<vtkInteractorStyleTrackballCamera> style;
    interactor->SetRenderWindow(window);
    interactor->SetInteractorStyle(style);
    window->Render();
    interactor->Start();
}

} // namespace

int main(int argc, char **argv)
{
    if (argc < 2)
    {
        std::cerr << "usage: " << argv[0] << " file.x3p\n";
        return 1;
    }
    try
    {
        Reader reader = create_reader();
        check(x3pio_open_file(reader.get(), argv[1]),
              x3pio_reader_error_message(reader.get()));
        Data data = read_data(reader.get());
        view(data);
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << "\n";
        return 1;
    }
}
