#ifndef sensei_VTKPosthocIO_h
#define sensei_VTKPosthocIO_h

#include "AnalysisAdaptor.h"
#include "DataRequirements.h"

#include <mpi.h>
#include <vector>
#include <string>

class vtkInformation;
class vtkCompositeDataSet;

namespace sensei
{
/// @class VTKPosthocIO
/// brief sensei::VTKPosthocIO is a AnalysisAdaptor that writes
/// data to disk. This can be useful for generating preview datasets
/// that allow configuration of Catalyst and/or Libsim scripts, or
/// for staging data on resources such as burst buffers. This adaptor
/// supports writing to VisIt(.visit) or ParaView(.pvd) compatible
/// format. One must provide a set of data requirments,
/// consisting of a list of meshes and the arrays to write from
/// each mesh. File names are derived using the output directory,
/// the mesh name, and the mode.
class VTKPosthocIO : public AnalysisAdaptor
{
public:
  static VTKPosthocIO* New();
  senseiTypeMacro(VTKPosthocIO, AnalysisAdaptor);

  // Run time configuration
  int SetOutputDir(const std::string &outputDir);

  enum {MODE_PARAVIEW=0, MODE_VISIT=1};
  int SetMode(int mode);

  int SetMode(std::string mode);

  /// data requirements tell the adaptor what to push
  /// if none are given then all data is pushed.
  int SetDataRequirements(const DataRequirements &reqs);

  int AddDataRequirement(const std::string &meshName,
    int association, const std::vector<std::string> &arrays);

  // SENSEI API
  bool Execute(DataAdaptor* data) override;
  int Finalize() override;

protected:
  VTKPosthocIO();
  ~VTKPosthocIO();

  VTKPosthocIO(const VTKPosthocIO&) = delete;
  void operator=(const VTKPosthocIO&) = delete;

private:
  std::string OutputDir;
  DataRequirements Requirements;
  int Mode;

  template<typename T>
  using NameMap = std::map<std::string, T>;

  NameMap<std::vector<double>> Time;
  NameMap<std::vector<long>> TimeStep;
  NameMap<std::vector<long>> NumBlocks;
  NameMap<std::string> BlockExt;
  NameMap<long> FileId;
  NameMap<int> HaveBlockInfo;
};

}
#endif
