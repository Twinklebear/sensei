from mpi4py import *
from multiprocessing import Process,Lock,Value
from sensei import VTKDataAdaptor,ADIOSDataAdaptor,ADIOSAnalysisAdaptor
import sys,os
import numpy as np
import vtk, vtk.util.numpy_support as vtknp
from time import sleep

comm = MPI.COMM_WORLD
rank = comm.Get_rank()
n_ranks = comm.Get_size()

def error_message(msg):
  sys.stderr.write('ERROR[%d] : %s\n'%(rank, msg))

def status_message(msg, io_rank=0):
  if rank == io_rank:
    sys.stderr.write('STATUS[%d] : %s\n'%(rank, msg))

def check_array(array):
  # checks that array[i] == i
  test_array = vtknp.vtk_to_numpy(array)
  n_vals = len(test_array)
  base_array = np.empty(n_vals, dtype=test_array.dtype)
  i = 0
  while i < n_vals:
    base_array[i] = i
    i += 1
  ids = np.where(base_array != test_array)[0]
  if len(ids):
    error_message('wrong values at %s'%(str(ids)))
    return -1
  return 0

def read_data(file_name, method):
  # initialize the data adaptor
  status_message('initializing ADIOSDataAdaptor %s %s'%(file_name,method))
  da = ADIOSDataAdaptor.New()
  da.Open(method, file_name)
  # process all time steps
  n_steps = 0
  retval = 0
  while True:
    # get the time info
    t = da.GetDataTime()
    it = da.GetDataTimeStep()
    status_message('received step %d time %0.1f'%(it, t))

    # get the mesh info
    nMeshes = da.GetNumberOfMeshes()
    i = 0
    while i < nMeshes:
      meshName = da.GetMeshName(i)
      status_message('received mesh %s'%(meshName))

      # get a VTK dataset with all the arrays
      ds = da.GetMesh(meshName, False)
      # request each array
      assocs = {vtk.VTK_POINT_DATA:'point', vtk.VTK_CELL_DATA:'cell'}
      for assoc,assoc_name in assocs.iteritems():
        n_arrays = da.GetNumberOfArrays(meshName, assoc)
        i = 0
        while i < n_arrays:
          array_name = da.GetArrayName(meshName, assoc, i)
          da.AddArray(ds, meshName, assoc, array_name)
          i += 1
      # this often will cause segv's if the dataset has been
      # improperly constructed, thus serves as a good check
      str_rep = str(ds)
      # check the arrays have the expected data
      it = ds.NewIterator()
      while not it.IsDoneWithTraversal():
        bds = it.GetCurrentDataObject()
        idx = it.GetCurrentFlatIndex()
        for assoc,assoc_name in assocs.iteritems():
          n_arrays = da.GetNumberOfArrays(meshName, assoc)
          status_message('checking %d %s data arrays ' \
            'in block %d %s'%(n_arrays,assoc_name,idx,bds.GetClassName()), \
            rank)
          j = 0
          while j < n_arrays:
            array = bds.GetPointData().GetArray(j) \
              if assoc == vtk.VTK_POINT_DATA else \
                bds.GetCellData().GetArray(j)
            if (check_array(array)):
              error_message('Test failed on array %d "%s"'%( \
                j, array.GetName()))
              retval = -1
            j += 1
        it.GoToNextItem()
      i += 1

    n_steps += 1
    if (da.Advance()):
      break

    # close down the stream
    da.Close()
    status_message('closed stream after receiving %d steps'%(n_steps))
    return retval

if __name__ == '__main__':
  # process command line
  file_name = sys.argv[1]
  method = sys.argv[2]
  # write data
  ierr = read_data(file_name, method)
  if ierr:
    error_message('read failed')
  # return the error code
  sys.exit(ierr)
