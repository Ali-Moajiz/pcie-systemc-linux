// pci-defs-fix.h
// Add this file and include it before pf-config.h

#ifndef PCI_DEFS_FIX_H
#define PCI_DEFS_FIX_H

// Missing PCI definitions that may not be in your headers
// Note: DO NOT define PCI_EXPROM_BAR - it's already defined in pcie_api.h as enum

#ifndef PCI_BASE_ADDRESS_MEM_TYPE_64
#define PCI_BASE_ADDRESS_MEM_TYPE_64 0x04
#endif

#ifndef PCI_EXP_DEVCAP_RBER
#define PCI_EXP_DEVCAP_RBER 0x00008000
#endif

#ifndef PCI_EXP_LNKCAP_SLS_2_5GB
#define PCI_EXP_LNKCAP_SLS_2_5GB 0x00000001
#endif

#ifndef PCI_EXP_LNKSTA_CLS_2_5GB
#define PCI_EXP_LNKSTA_CLS_2_5GB 0x0001
#endif

#ifndef PCI_EXP_LNKSTA_NLW_X1
#define PCI_EXP_LNKSTA_NLW_X1 0x0010
#endif

#endif // PCI_DEFS_FIX_H