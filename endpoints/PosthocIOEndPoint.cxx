/*!
 * This program is designed to be a posthoc endpoint to read datasets written out using sensei::PosthocIO.
 */
#include <opts/opts.h>
#include <mpi.h>
#include <iostream>
#include <VTKDataAdaptor.h>
#include <ConfigurableAnalysis.h>
#include <Timer.h>
#include <vtkDataSet.h>
#include <vtkNew.h>
#include <vtkSmartPointer.h>
#include <vtkXMLMultiBlockDataReader.h>

using std::cout;
using std::cerr;
using std::endl;

int main(int argc, char** argv)
{
  int rank, size;
  MPI_Comm comm = MPI_COMM_WORLD;
  MPI_Init (&argc, &argv);
  MPI_Comm_rank(comm, &rank);
  MPI_Comm_size(comm, &size);

  std::string input_pattern;
  std::string config_file;
  int count=1, begin=0, step=1;

  opts::Options ops(argc, argv);
  ops >> opts::Option('f', "config", config_file, "Sensei analysis configuration xml (required).")
      >> opts::Option('p', "pattern", input_pattern, "Filename pattern (sprintf) for *.vtmb files (required).")
      >> opts::Option('c', "count", count, "Number of timesteps to read.")
      >> opts::Option('b', "begin", begin, "Start timestep.")
      >> opts::Option('s', "step", step, "Step size i.e. number of timesteps to skip (>=1).");

  bool log = ops >> opts::Present("log", "generate time and memory usage log");
  bool shortlog = ops >> opts::Present("shortlog", "generate a summary time and memory usage log");
  if (ops >> opts::Present('h', "help", "show help") ||
    input_pattern.empty() || config_file.empty() ||
    count <= 0 || step < 1)
    {
    if (rank == 0)
      {
      cout << "Usage: " << argv[0] << " [OPTIONS]\n\n" << ops;
      }
    MPI_Barrier(comm);
    MPI_Finalize();
    return 1;
    }

  timer::SetLogging(log || shortlog);
  timer::SetTrackSummariesOverTime(shortlog);

  vtkSmartPointer<sensei::ConfigurableAnalysis> analysis =
    vtkSmartPointer<sensei::ConfigurableAnalysis>::New();

  if (analysis->Initialize(comm, config_file))
    {
    SENSEI_ERROR("Failed to initialize the analysis")
    MPI_Abort(comm, 1);
    }

  size_t fname_length = input_pattern.size() + 128;
  char *fname = new char[fname_length];
  vtkNew<sensei::VTKDataAdaptor> dataAdaptor;
  for (int cc=0; cc < count; cc++)
    {
    int t_step = begin + cc * step;
    double t = static_cast<double>(t_step);
    timer::MarkStartTimeStep(t_step, t);

    snprintf(fname, fname_length, input_pattern.c_str(), t_step);
    if (true)
      {
      vtkNew<vtkXMLMultiBlockDataReader> reader;
      reader->SetFileName(fname);
      reader->ReadFromInputStringOn();
      timer::MarkStartEvent("posthoc::pre-read");
      // Since vtkXMLMultiBlockDataReader tries to read the XML meta-file on all ranks,
      // we explicitly broadcast the xml file to all ranks.
      if (rank == 0)
        {
        std::ifstream t(fname);
        std::stringstream buffer;
        buffer << t.rdbuf();
        std::string data = buffer.str();
        unsigned int length = static_cast<unsigned int>(data.size()) + 1;
        MPI_Bcast(&length, 1, MPI_UNSIGNED, 0, comm);
        MPI_Bcast(const_cast<char*>(data.c_str()), length, MPI_CHAR, 0, comm);
        reader->SetInputString(data);
        }
      else
        {
        unsigned int length;
        MPI_Bcast(&length, 1, MPI_UNSIGNED, 0, comm);
        char* data = new char[length];
        MPI_Bcast(data, length, MPI_CHAR, 0, comm);
        reader->SetInputString(data);
        delete [] data;
        }
      timer::MarkEndEvent("posthoc::pre-read");

      timer::MarkStartEvent("posthoc::read");

#if VTK_MAJOR_VERSION > 7 || (VTK_MAJOR_VERSION == 7 && VTK_MINOR_VERSION >= 1)
      // Use API added in 7.1
      reader->UpdatePiece(rank, size, 0);
#else
      // Using old API here since I'm not sure which VTK we'll have on our test
      // runs.
      reader->UpdateInformation();
      reader->SetUpdateExtent(0, rank, size, 0);
      reader->Update();
#endif
      timer::MarkEndEvent("posthoc::read");
      dataAdaptor->SetDataObject(reader->GetOutputDataObject(0));
      }

    timer::MarkStartEvent("adios::analysis");
    analysis->Execute(dataAdaptor.GetPointer());
    timer::MarkEndEvent("adios::analysis");

    dataAdaptor->ReleaseData();
    timer::MarkEndTimeStep();
    }
  delete [] fname;

  timer::MarkStartEvent("adios::finalize");
  analysis = NULL;
  timer::MarkEndEvent("adios::finalize");

  timer::PrintLog(std::cout, comm);
  MPI_Finalize();
  return 0;
}
