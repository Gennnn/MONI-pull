The way this is organized:

Each folder is a "Module"

A module is a collection of components & systems, typically one file per component.
This file defines the component and ANY systems that EDIT the components in that specific file.
Other components can/should be READ but not written to.

Each defined component/system should exist within a namespaced defined by the enclosing module.

CCL.h (Connect Component Logic) is the secret sauce that enables code to be run directly in the definition files.
We use this ability to connect component functions directly to EnTT through the use of "Signals" in the library.

This makes the architecture very efficient and controllable since component updates must be triggered directly.   

