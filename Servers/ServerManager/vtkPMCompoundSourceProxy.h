/*=========================================================================

  Program:   ParaView
  Module:    vtkPMCompoundSourceProxy

  Copyright (c) Kitware, Inc.
  All rights reserved.
  See Copyright.txt or http://www.paraview.org/HTML/Copyright.html for details.

     This software is distributed WITHOUT ANY WARRANTY; without even
     the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
     PURPOSE.  See the above copyright notice for more information.

=========================================================================*/
// .NAME vtkPMCompoundSourceProxy
// .SECTION Description
// vtkPMCompoundSourceProxy is the server-side helper for a vtkP=SMCompoundSourceProxy
// It provide the mapping to the exposed output port to the underneath internal
// sub-proxy.

#ifndef __vtkPMCompoundSourceProxy_h
#define __vtkPMCompoundSourceProxy_h

#include "vtkPMSourceProxy.h"

class vtkAlgorithm;

class VTK_EXPORT vtkPMCompoundSourceProxy : public vtkPMSourceProxy
{
public:
  static vtkPMCompoundSourceProxy* New();
  vtkTypeMacro(vtkPMCompoundSourceProxy, vtkPMSourceProxy);
  void PrintSelf(ostream& os, vtkIndent indent);

  // Description:
  // Returns the vtkClientServerID for an output port, if valid.
  virtual vtkClientServerID GetOutputPortID(int port);

//BTX
protected:
  vtkPMCompoundSourceProxy();
  ~vtkPMCompoundSourceProxy();

  // Description:
  // Read xml-attributes.
  virtual bool ReadXMLAttributes(vtkPVXMLElement* element);

  // Description:
  // Create the output ports and add post filters for each output port.
  virtual bool CreateOutputPorts();

  // Description:
  // Triggers UpdateInformation(). Overridden to call UpdateInformation() on the
  // internal "real" source proxies.
  virtual void UpdateInformation();

private:
  vtkPMCompoundSourceProxy(const vtkPMCompoundSourceProxy&); // Not implemented
  void operator=(const vtkPMCompoundSourceProxy&); // Not implemented

  class vtkInternals;
  vtkInternals* Internals;
//ETX
};

#endif
