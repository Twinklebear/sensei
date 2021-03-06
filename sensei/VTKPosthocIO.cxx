#include "VTKPosthocIO.h"
#include "senseiConfig.h"
#include "DataAdaptor.h"
#include "VTKUtils.h"
#include "Error.h"

#include <vtkCellData.h>
#include <vtkCompositeDataIterator.h>
#include <vtkMultiBlockDataSet.h>
#include <vtkDataArray.h>
#include <vtkDataArrayTemplate.h>
#include <vtkDataObject.h>
#include <vtkDataSetAttributes.h>
#include <vtkImageData.h>
#include <vtkPolyData.h>
#include <vtkRectilinearGrid.h>
#include <vtkStructuredGrid.h>
#include <vtkUnstructuredGrid.h>
#include <vtkInformation.h>
#include <vtkObjectFactory.h>
#include <vtkPointData.h>
#include <vtkSmartPointer.h>

#include <algorithm>
#include <sstream>
#include <fstream>
#include <cassert>

#include <vtkAlgorithm.h>
#include <vtkCompositeDataPipeline.h>
#include <vtkXMLDataSetWriter.h>

#include <mpi.h>

using vtkCompositeDataSetPtr = vtkSmartPointer<vtkCompositeDataSet>;

//-----------------------------------------------------------------------------
static
std::string getBlockExtension(vtkDataObject *dob)
{
  if (dynamic_cast<vtkPolyData*>(dob))
  {
    return ".vtp";
  }
  else if (dynamic_cast<vtkUnstructuredGrid*>(dob))
  {
    return ".vtu";
  }
  else if (dynamic_cast<vtkImageData*>(dob))
  {
    return ".vti";
  }
  else if (dynamic_cast<vtkRectilinearGrid*>(dob))
  {
    return ".vtr";
  }
  else if (dynamic_cast<vtkStructuredGrid*>(dob))
  {
    return ".vts";
  }
  else if (dynamic_cast<vtkMultiBlockDataSet*>(dob))
  {
    return ".vtm";
  }

  SENSEI_ERROR("Failed to determine file extension for \""
    << dob->GetClassName() << "\"")
  return "";
}

//-----------------------------------------------------------------------------
static
std::string getBlockFileName(const std::string &outputDir,
  const std::string &meshName, long blockId, long fileId,
  const std::string &blockExt)
{
  std::ostringstream oss;

  oss << outputDir << "/" << meshName << "_"
    << std::setw(6) << std::setfill('0') << blockId << "_"
    << std::setw(6) << std::setfill('0') << fileId << blockExt;

  return oss.str();
}

namespace sensei
{
//-----------------------------------------------------------------------------
senseiNewMacro(VTKPosthocIO);

//-----------------------------------------------------------------------------
VTKPosthocIO::VTKPosthocIO() : OutputDir("./"), Mode(MODE_PARAVIEW)
{}

//-----------------------------------------------------------------------------
VTKPosthocIO::~VTKPosthocIO()
{}

//-----------------------------------------------------------------------------
int VTKPosthocIO::SetOutputDir(const std::string &outputDir)
{
  this->OutputDir = outputDir;
  return 0;
}

//-----------------------------------------------------------------------------
int VTKPosthocIO::SetMode(int mode)
{
  if (!(mode == VTKPosthocIO::MODE_VISIT) ||
    (mode == VTKPosthocIO::MODE_PARAVIEW))
    {
    SENSEI_ERROR("Invalid mode " << mode)
    return -1;
    }

  this->Mode = mode;
  return 0;
}

//-----------------------------------------------------------------------------
int VTKPosthocIO::SetMode(std::string modeStr)
{
  unsigned int n = modeStr.size();
  for (unsigned int i = 0; i < n; ++i)
    modeStr[i] = tolower(modeStr[i]);

  int mode = 0;
  if (modeStr == "visit")
    {
    mode = VTKPosthocIO::MODE_VISIT;
    }
  else if (modeStr == "paraview")
    {
    mode = VTKPosthocIO::MODE_PARAVIEW;
    }
  else
    {
    SENSEI_ERROR("invalid mode \"" << modeStr << "\"")
    return -1;
    }

  this->Mode = mode;
  return 0;
}

//-----------------------------------------------------------------------------
int VTKPosthocIO::SetDataRequirements(const DataRequirements &reqs)
{
  this->Requirements = reqs;
  return 0;
}

//-----------------------------------------------------------------------------
int VTKPosthocIO::AddDataRequirement(const std::string &meshName,
  int association, const std::vector<std::string> &arrays)
{
  this->Requirements.AddRequirement(meshName, association, arrays);
  return 0;
}


//-----------------------------------------------------------------------------
bool VTKPosthocIO::Execute(DataAdaptor* dataAdaptor)
{
  // if no dataAdaptor requirements are given, push all the data
  // fill in the requirements with every thing
  if (this->Requirements.Empty())
    {
    if (this->Requirements.Initialize(dataAdaptor))
      {
      SENSEI_ERROR("Failed to initialze dataAdaptor description")
      return false;
      }
    SENSEI_WARNING("No subset specified. Writing all available data")
    }

  MeshRequirementsIterator mit =
    this->Requirements.GetMeshRequirementsIterator();

  for (; mit; ++mit)
    {
    // get the mesh
    vtkDataObject* dobj = nullptr;
    std::string meshName = mit.MeshName();
    if (dataAdaptor->GetMesh(meshName, mit.StructureOnly(), dobj))
      {
      SENSEI_ERROR("Failed to get mesh \"" << meshName << "\"")
      return false;
      }

    // get ghost cell/node metadata always provide this information as
    // it is essential to process the data objects
    int nGhostCellLayers = 0;
    int nGhostNodeLayers = 0;
    if (dataAdaptor->GetMeshHasGhostCells(mit.MeshName(), nGhostCellLayers) ||
      dataAdaptor->GetMeshHasGhostNodes(mit.MeshName(), nGhostNodeLayers))
      {
      SENSEI_ERROR("Failed to get ghost layer info for mesh \"" << mit.MeshName() << "\"")
      return false;
      }

    // add the ghost cell arrays to the mesh
    if ((nGhostCellLayers > 0) && dataAdaptor->AddGhostCellsArray(dobj, mit.MeshName()))
      {
      SENSEI_ERROR("Failed to get ghost cells for mesh \"" << mit.MeshName() << "\"")
      return false;
      }

    // add the ghost node arrays to the mesh
    if ((nGhostNodeLayers > 0) && dataAdaptor->AddGhostNodesArray(dobj, mit.MeshName()))
      {
      SENSEI_ERROR("Failed to get ghost nodes for mesh \"" << mit.MeshName() << "\"")
      return false;
      }

    // add the required arrays
    ArrayRequirementsIterator ait =
      this->Requirements.GetArrayRequirementsIterator(meshName);

    for (; ait; ++ait)
      {
      if (dataAdaptor->AddArray(dobj, mit.MeshName(),
         ait.Association(), ait.Array()))
        {
        SENSEI_ERROR("Failed to add "
          << VTKUtils::GetAttributesName(ait.Association())
          << " data array \"" << ait.Array() << "\" to mesh \""
          << meshName << "\"")
        return false;
        }
      }

    // This class does not use VTK's parallel writers because at this
    // time those writers gather some data to rank 0 and this results
    // in OOM crashes when run with 45k cores on Cori.

    // make sure we have composite dataset if not create one
    int rank = 0;
    int nRanks = 1;

    MPI_Comm_rank(this->GetCommunicator(), &rank);
    MPI_Comm_size(this->GetCommunicator(), &nRanks);

    vtkCompositeDataSetPtr cd;
    if (dynamic_cast<vtkCompositeDataSet*>(dobj))
      {
      cd = static_cast<vtkCompositeDataSet*>(dobj);
      }
    else
      {
      vtkMultiBlockDataSet *mb = vtkMultiBlockDataSet::New();
      mb->SetNumberOfBlocks(nRanks);
      mb->SetBlock(rank, dobj);
      cd.TakeReference(mb);
      }

    vtkCompositeDataIterator *it = cd->NewIterator();
    it->SetSkipEmptyNodes(1);

    // figure out block distribution, assume that it does not change, and
    // that block types are homgeneous
    if (!this->HaveBlockInfo[meshName])
      {
      if (!it->IsDoneWithTraversal())
        this->BlockExt[meshName] = getBlockExtension(it->GetCurrentDataObject());

      this->FileId[meshName] = 0;
      this->HaveBlockInfo[meshName] = 1;
      }

    // compute the number of blocks, this could change in time
    long nBlocks = 0;
    it->SetSkipEmptyNodes(0);
    for (it->InitTraversal(); !it->IsDoneWithTraversal(); it->GoToNextItem())
      ++nBlocks;
    it->SetSkipEmptyNodes(1);

    // write the blocks
    vtkXMLDataSetWriter *writer = vtkXMLDataSetWriter::New();
    for (it->InitTraversal(); !it->IsDoneWithTraversal(); it->GoToNextItem())
      {
      long blockId = std::max(0u, it->GetCurrentFlatIndex() - 1);

      std::string fileName =
        getBlockFileName(this->OutputDir, meshName, blockId,
          this->FileId[meshName], this->BlockExt[meshName]);

      writer->SetInputData(it->GetCurrentDataObject());
      writer->SetDataModeToAppended();
      writer->EncodeAppendedDataOff();
      writer->SetCompressorTypeToNone();
      writer->SetFileName(fileName.c_str());
      writer->Write();
      }
    writer->Delete();

    this->FileId[meshName] += 1;

    // rank 0 keeps track of time info for meta file
    if (rank == 0)
      {
      double time = dataAdaptor->GetDataTime();
      this->Time[meshName].push_back(time);

      long step = dataAdaptor->GetDataTimeStep();
      this->TimeStep[meshName].push_back(step);

      this->NumBlocks[meshName].push_back(nBlocks);
      }
    }

  return true;
}

//-----------------------------------------------------------------------------
int VTKPosthocIO::Finalize()
{
  int rank = 0;
  MPI_Comm_rank(this->GetCommunicator(), &rank);

  if (rank != 0)
    return 0;

  int nRanks = 1;
  MPI_Comm_size(this->GetCommunicator(), &nRanks);

  std::vector<std::string> meshNames;
  this->Requirements.GetRequiredMeshes(meshNames);

  unsigned int nMeshes = meshNames.size();
  for (unsigned int i = 0; i < nMeshes; ++i)
    {
    const std::string &meshName = meshNames[i];

    if (this->HaveBlockInfo.find(meshName) == this->HaveBlockInfo.end())
      {
      SENSEI_ERROR("No blocks have been written for a mesh named \""
        << meshName << "\"")
      return -1;
      }

    const std::vector<long> &numBlocks = this->NumBlocks[meshName];

    std::vector<double> &times = this->Time[meshName];
    long nSteps = times.size();

    std::string &blockExt = this->BlockExt[meshName];

    if (this->Mode == VTKPosthocIO::MODE_PARAVIEW)
      {
      std::string pvdFileName = this->OutputDir + "/" + meshName + ".pvd";
      ofstream pvdFile(pvdFileName);

      if (!pvdFile)
        {
        SENSEI_ERROR("Failed to open " << pvdFileName << " for writing")
        return -1;
        }

      pvdFile << "<?xml version=\"1.0\"?>" << endl
        << "<VTKFile type=\"Collection\" version=\"0.1\""
           " byte_order=\"LittleEndian\" compressor=\"\">" << endl
        << "<Collection>" << endl;

      for (long i = 0; i < nSteps; ++i)
        {
        long nBlocks = numBlocks[i];
        for (long j = 0; j < nBlocks; ++j)
          {
          std::string fileName =
            getBlockFileName(this->OutputDir, meshName, j, i, blockExt);

          pvdFile << "<DataSet timestep=\"" << times[i]
            << "\" group=\"\" part=\"" << j << "\" file=\"" << fileName
            << "\"/>" << endl;
          }
        }

      pvdFile << "</Collection>" << endl
        << "</VTKFile>" << endl;

      return 0;
      }
    else if (this->Mode == VTKPosthocIO::MODE_VISIT)
      {
      std::string visitFileName = this->OutputDir + "/" + meshName + ".visit";
      ofstream visitFile(visitFileName);

      if (!visitFile)
        {
        SENSEI_ERROR("Failed to open " << visitFileName << " for writing")
        return -1;
        }

      // TODO -- does .visit file support a changing number of blocks?
      long nBlocks = numBlocks[0];
      visitFile << "!NBLOCKS " << nBlocks << endl;

      for (long i = 0; i < nSteps; ++i)
        {
        visitFile << "!TIME " << times[i] << endl;
        }

      for (long i = 0; i < nSteps; ++i)
        {
        for (long j = 0; j < nBlocks; ++j)
          {
          std::string fileName =
            getBlockFileName(this->OutputDir, meshName, j, i, blockExt);

          visitFile << fileName << endl;
          }
        }

      }
    else
      {
      SENSEI_ERROR("Invalid mode \"" << this->Mode << "\"")
      return -1;
      }
    }
  return 0;
}

}
