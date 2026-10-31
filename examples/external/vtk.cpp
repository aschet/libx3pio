// Display an x3p file with VTK.

#include <x3pio/file.h>
#include <x3pio/x3pio.h>

#include <vtkActor.h>
#include <vtkCamera.h>
#include <vtkDataSetAttributes.h>
#include <vtkGeometryFilter.h>
#include <vtkImageData.h>
#include <vtkInteractorStyleTrackballCamera.h>
#include <vtkLookupTable.h>
#include <vtkNew.h>
#include <vtkPointData.h>
#include <vtkPolyData.h>
#include <vtkPolyDataMapper.h>
#include <vtkRenderWindow.h>
#include <vtkRenderWindowInteractor.h>
#include <vtkRenderer.h>
#include <vtkSmartPointer.h>
#include <vtkUnsignedCharArray.h>
#include <vtkWarpScalar.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

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

float *heights_of(vtkImageData *image)
{
    return static_cast<float *>(image->GetScalarPointer());
}

// The rows of vtkImageData start at y = 0 and the coordinate system is
// right-handed, as in the standard, so nothing is flipped and the heights are
// read into the array of the image.
vtkSmartPointer<vtkImageData> read_image(x3pio_reader *reader)
{
    const x3pio_document *document = x3pio_reader_document(reader);
    auto image = vtkSmartPointer<vtkImageData>::New();
    image->SetDimensions(static_cast<int>(x3pio_document_columns(document)),
                         static_cast<int>(x3pio_document_rows(document)), 1);
    image->SetSpacing(x3pio_document_axis(document, X3PIO_X).increment,
                      x3pio_document_axis(document, X3PIO_Y).increment, 1.0);
    image->SetOrigin(0.0, 0.0, 0.0);
    image->AllocateScalars(VTK_FLOAT, 1);
    check(x3pio_read_heights(reader, x3pio_layout_packed(X3PIO_TYPE_FLOAT),
                             heights_of(image)),
          x3pio_reader_error_message(reader));
    return image;
}

void hide_points_not_measured(vtkImageData *image)
{
    const vtkIdType count = image->GetNumberOfPoints();
    const float *heights = heights_of(image);
    vtkNew<vtkUnsignedCharArray> hidden;
    hidden->SetName(vtkDataSetAttributes::GhostArrayName());
    hidden->SetNumberOfValues(count);
    for (vtkIdType i = 0; i < count; ++i)
    {
        hidden->SetValue(
            i, std::isnan(heights[i]) ? vtkDataSetAttributes::HIDDENPOINT : 0);
    }
    image->GetPointData()->AddArray(hidden);
}

// The height field: the polygons of the image, warped by the scalars times a
// factor. The hidden points are set to 0 in a copy, as a NaN would reach the
// bounds.
vtkSmartPointer<vtkPolyData> height_field(vtkImageData *image, double factor)
{
    vtkNew<vtkImageData> patched;
    patched->DeepCopy(image);
    float *heights = heights_of(patched);
    const vtkIdType count = patched->GetNumberOfPoints();
    for (vtkIdType i = 0; i < count; ++i)
    {
        heights[i] = std::isnan(heights[i]) ? 0.0F : heights[i];
    }
    vtkNew<vtkGeometryFilter> surface;
    surface->SetInputData(patched);
    vtkNew<vtkWarpScalar> warp;
    warp->SetInputConnection(surface->GetOutputPort());
    warp->SetScaleFactor(factor);
    warp->UseNormalOff();
    warp->SetNormal(0.0, 0.0, 1.0);
    warp->Update();
    return vtkPolyData::SafeDownCast(warp->GetOutput());
}

std::array<double, 2> z_range(vtkImageData *image)
{
    double bounds[6];
    height_field(image, 1.0)->GetBounds(bounds);
    return {bounds[4], bounds[5]};
}

// The relief is raised to a quarter of the width of the surface, as a few
// micrometres of height would not be seen.
void view(vtkImageData *image)
{
    const std::array<double, 2> z = z_range(image);
    double bounds[6];
    image->GetBounds(bounds);
    const double width = std::max(bounds[1] - bounds[0], bounds[3] - bounds[2]);
    const double raise = z[1] > z[0] ? 0.25 * width / (z[1] - z[0]) : 1.0;

    vtkNew<vtkPolyDataMapper> mapper;
    mapper->SetInputData(height_field(image, raise));
    vtkNew<vtkLookupTable> colours;
    colours->SetHueRange(0.667, 0.0);
    colours->Build();
    mapper->SetLookupTable(colours);
    mapper->SetScalarRange(z[0], z[1]);
    vtkNew<vtkActor> actor;
    actor->SetMapper(mapper);
    vtkNew<vtkRenderer> renderer;
    renderer->AddActor(actor);
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
    const char *path = argv[1];
    try
    {
        Reader reader = create_reader();
        check(x3pio_open_file(reader.get(), path),
              x3pio_reader_error_message(reader.get()));
        vtkSmartPointer<vtkImageData> image = read_image(reader.get());
        hide_points_not_measured(image);
        view(image);
    }
    catch (const std::exception &error)
    {
        std::cerr << error.what() << "\n";
        return 1;
    }
}
