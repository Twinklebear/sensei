#include "VTKUtils.h"
#include "Error.h"

#include <vtkDataObject.h>
#include <vtkDataSet.h>
#include <vtkCompositeDataSet.h>
#include <vtkCompositeDataIterator.h>
#include <vtkDataSetAttributes.h>
#include <vtkFieldData.h>
#include <vtkCellData.h>
#include <vtkPointData.h>
#include <vtkObjectBase.h>
#include <vtkObject.h>
#include <vtkDataArray.h>
#include <vtkAbstractArray.h>
#include <vtkSmartPointer.h>
#include <vtkInformation.h>
#include <vtkInformationIntegerKey.h>
#include <vtkIntArray.h>

#include <functional>

using vtkDataObjectPtr = vtkSmartPointer<vtkDataObject>;
using vtkCompositeDataIteratorPtr = vtkSmartPointer<vtkCompositeDataIterator>;

namespace sensei
{
namespace VTKUtils
{

//----------------------------------------------------------------------------
int GetAssociation(std::string assocStr, int &assoc)
{
  unsigned int n = assocStr.size();
  for (unsigned int i = 0; i < n; ++i)
    assocStr[i] = tolower(assocStr[i]);

  if (assocStr == "point")
    {
    assoc = vtkDataObject::POINT;
    return 0;
    }
  else if (assocStr == "cell")
    {
    assoc = vtkDataObject::CELL;
    return 0;
    }
  else if (assocStr == "field")
    {
    assoc = vtkDataObject::FIELD;
    return 0;
    }

  SENSEI_ERROR("Invalid association \"" << assocStr << "\"")
  return -1;
}

//----------------------------------------------------------------------------
const char *GetAttributesName(int association)
{
  switch (association)
    {
    case vtkDataObject::POINT:
      return "point";
      break;
    case vtkDataObject::CELL:
      return "cell";
      break;
    case vtkDataObject::FIELD:
      return "field";
      break;
    }
  SENSEI_ERROR("Invalid data set attributes association")
  return "";
}

//----------------------------------------------------------------------------
vtkFieldData *GetAttributes(vtkDataSet *dobj, int association)
{
  switch (association)
    {
    case vtkDataObject::POINT:
      return static_cast<vtkFieldData*>(dobj->GetPointData());
      break;
    case vtkDataObject::CELL:
      return static_cast<vtkFieldData*>(dobj->GetCellData());
      break;
    case vtkDataObject::FIELD:
      return static_cast<vtkFieldData*>(dobj->GetFieldData());
      break;
    }
  SENSEI_ERROR("Invalid data set attributes association")
  return nullptr;
}

//----------------------------------------------------------------------------
int Apply(vtkCompositeDataSet *cd, vtkCompositeDataSet *cdo,
  BinaryDatasetFunction &func)
{
  vtkCompositeDataIteratorPtr cdit;
  cdit.TakeReference(cd->NewIterator());
  while (!cdit->IsDoneWithTraversal())
    {
    vtkDataObject *obj = cd->GetDataSet(cdit);
    vtkDataObject *objOut = cdo->GetDataSet(cdit);
    // recurse through nested composite datasets
    if (vtkCompositeDataSet *cdn = dynamic_cast<vtkCompositeDataSet*>(obj))
      {
      vtkCompositeDataSet*cdnOut = static_cast<vtkCompositeDataSet*>(objOut);
      int ret = Apply(cdn, cdnOut, func);
      if (ret < 0)
        {
        // stop with error
        SENSEI_ERROR("Failed to apply to composite data set index "
          << cdit->GetCurrentFlatIndex())
        return -1;
        }
      else if (ret > 0)
        {
        // stop without error
        return 1;
        }
      }
    // process data set leaves
    else if(vtkDataSet *ds = dynamic_cast<vtkDataSet*>(obj))
      {
      vtkDataSet *dsOut = static_cast<vtkDataSet*>(objOut);
      int ret = func(ds, dsOut);
      if (ret < 0)
        {
        // stop with error
        SENSEI_ERROR("Function failed in apply at data set index "
          << cdit->GetCurrentFlatIndex())
        return -1;
        }
      else if (ret)
        {
        // stop without error
        return 1;
        }
      }
    else if (obj)
      {
      SENSEI_ERROR("Can't apply to " << obj->GetClassName())
      return -1;
      }
    cdit->GoToNextItem();
    }
  return 0;
}

//----------------------------------------------------------------------------
int Apply(vtkDataObject *dobj, vtkDataObject *dobjo,
  BinaryDatasetFunction &func)
{
  if (vtkCompositeDataSet *cd = dynamic_cast<vtkCompositeDataSet*>(dobj))
    {
    vtkCompositeDataSet *cdo = static_cast<vtkCompositeDataSet*>(dobjo);
    if (Apply(cd, cdo, func) < 0)
      {
      return -1;
      }
    }
  else if (vtkDataSet *ds = dynamic_cast<vtkDataSet*>(dobj))
    {
    vtkDataSet *dso = static_cast<vtkDataSet*>(dobjo);
    if (func(ds, dso) < 0)
      {
      return -1;
      }
    }
  else
    {
    SENSEI_ERROR("Unsupoorted data object type " << dobj->GetClassName())
    return -1;
    }

  return 0;
}

//----------------------------------------------------------------------------
int Apply(vtkCompositeDataSet *cd, DatasetFunction &func)
{
  vtkCompositeDataIteratorPtr cdit;
  cdit.TakeReference(cd->NewIterator());
  while (!cdit->IsDoneWithTraversal())
    {
    vtkDataObject *obj = cd->GetDataSet(cdit);
    // recurse through nested composite datasets
    if (vtkCompositeDataSet *cdn = dynamic_cast<vtkCompositeDataSet*>(obj))
      {
      int ret = Apply(cdn, func);
      if (ret < 0)
        {
        // stop with error
        SENSEI_ERROR("Failed to apply to composite data set index "
          << cdit->GetCurrentFlatIndex())
        return -1;
        }
      else if (ret > 0)
        {
        // stop without error
        return 1;
        }
      }
    // process data set leaves
    else if(vtkDataSet *ds = dynamic_cast<vtkDataSet*>(obj))
      {
      int ret = func(ds);
      if (ret < 0)
        {
        // stop with error
        SENSEI_ERROR("Function failed to apply to composite data set index "
          << cdit->GetCurrentFlatIndex())
        return -1;
        }
      else if (ret)
        {
        // stop without error
        return 1;
        }
      }
    else if (obj)
      {
      SENSEI_ERROR("Can't apply to " << obj->GetClassName())
      return -1;
      }
    cdit->GoToNextItem();
    }
  return 0;
}

//----------------------------------------------------------------------------
int Apply(vtkDataObject *dobj, DatasetFunction &func)
{
  if (vtkCompositeDataSet *cd = dynamic_cast<vtkCompositeDataSet*>(dobj))
    {
    if (Apply(cd, func) < 0)
      {
      return -1;
      }
    }
  else if (vtkDataSet *ds = dynamic_cast<vtkDataSet*>(dobj))
    {
    if (func(ds) < 0)
      {
      return -1;
      }
    }
  else
    {
    SENSEI_ERROR("Unsupoorted data object type " << dobj->GetClassName())
    return -1;
    }
  return 0;
}

//----------------------------------------------------------------------------
int GetGhostLayerMetadata(vtkDataObject *mesh,
  int &nGhostCellLayers, int &nGhostNodeLayers)
{
  // get the ghost layer metadata
  vtkFieldData *fd = mesh->GetFieldData();

  vtkIntArray *glmd =
    dynamic_cast<vtkIntArray*>(fd->GetArray("senseiGhostLayers"));

  if (!glmd)
    return -1;

  nGhostCellLayers = glmd->GetValue(0);
  nGhostNodeLayers = glmd->GetValue(1);

  return 0;
}

//----------------------------------------------------------------------------
int SetGhostLayerMetadata(vtkDataObject *mesh,
  int nGhostCellLayers, int nGhostNodeLayers)
{
  // pass ghost layer metadata in field data.
  vtkIntArray *glmd = vtkIntArray::New();
  glmd->SetName("senseiGhostLayers");
  glmd->SetNumberOfTuples(2);
  glmd->SetValue(0, nGhostCellLayers);
  glmd->SetValue(1, nGhostNodeLayers);

  vtkFieldData *fd = mesh->GetFieldData();
  fd->AddArray(glmd);
  glmd->Delete();

  return 0;
}

}
}
