#ifndef TSUNAMI_LAB_DISPLACEMENT_DISPLACEMENTMODEL_H
#define TSUNAMI_LAB_DISPLACEMENT_DISPLACEMENTMODEL_H

namespace tsunami_lab {
namespace displacement {

// Abstract vertical seafloor displacement of a seismic source, evaluated in a
// local metre frame centred on the source (+east, +north). The query is kept
// minimal so a future Okada dislocation model can implement the same interface.
class DisplacementModel {
public:
  virtual ~DisplacementModel() = default;

  // Vertical displacement in metres (positive = uplift) at the local offset
  // (i_east, i_north) metres from the source.
  virtual double verticalDisplacement(double i_east, double i_north) const = 0;

  // Radius in metres beyond which the displacement is negligible.
  virtual double influenceRadius() const = 0;
};

} // namespace displacement
} // namespace tsunami_lab

#endif
