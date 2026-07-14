//{{NO_DEPENDENCIES}}
// resource.h - GlimRegionViewer
// ASCII only, no BOM (included by the .rc which rc.exe reads as CP949).
// Most controls are created dynamically in OnInitDialog; IDs are defined here.

#define IDD_GLIMREGIONVIEWER_DIALOG     100

// Static control that exists in the .rc
#define IDC_STATIC_STATUS               1000

// Dynamically created controls (Create() in OnInitDialog)
#define IDC_BTN_OPEN_IMAGE              1001
#define IDC_BTN_OPEN_FOLDER             1002
#define IDC_COMBO_ZOOM                  1003
#define IDC_CHECK_OVERLAY               1004
#define IDC_COMBO_PROFILE               1005
#define IDC_BTN_EXPORT_CSV              1006
#define IDC_LIST_FILES                  1007
#define IDC_LIST_FEATURES               1009
#define IDC_STATIC_ZOOM_LABEL           1010
#define IDC_STATIC_PROFILE_LABEL        1011
#define IDC_COMBO_BINARIZE              1012
#define IDC_STATIC_BINARIZE_LABEL       1013

// Tab control + dashboard controls (all created dynamically)
#define IDC_TAB_MAIN                    1020
#define IDC_STATIC_THREADS_LABEL        1021
#define IDC_EDIT_THREADS                1022
#define IDC_STATIC_XSCALE_LABEL         1023
#define IDC_EDIT_XSCALE                 1024
#define IDC_STATIC_YSCALE_LABEL         1025
#define IDC_EDIT_YSCALE                 1026
#define IDC_STATIC_TH_LABEL             1027
#define IDC_EDIT_TH                     1028
#define IDC_STATIC_OFFSET_LABEL         1029
#define IDC_EDIT_OFFSET                 1030
#define IDC_STATIC_WK_LABEL             1031
#define IDC_EDIT_WK_KERNEL              1032
#define IDC_STATIC_WB_LABEL             1033
#define IDC_EDIT_WK_BLUR                1034
#define IDC_STATIC_WR_LABEL             1035
#define IDC_EDIT_WK_RESP                1036
#define IDC_BTN_ANALYZE                 1037
#define IDC_PREVIEW_PANEL               1038
#define IDC_CARD_LIST                   1039
#define IDC_STATIC_HISTFEAT_LABEL       1040
#define IDC_COMBO_HISTFEAT              1041
#define IDC_CHART_PANEL                 1042
#define IDC_STATIC_HOME                 1043
#define IDC_IMAGE_VIEW                  1044

// Projection(inspector-style) binarize parameters (created dynamically)
#define IDC_STATIC_PBLACK_LABEL         1045
#define IDC_EDIT_PBLACK_TH              1046
#define IDC_STATIC_PWHITE_LABEL         1047
#define IDC_EDIT_PWHITE_TH              1048
#define IDC_STATIC_PKERNEL_LABEL        1049
#define IDC_EDIT_PKERNEL                1050

#ifdef APSTUDIO_INVOKED
#ifndef APSTUDIO_READONLY_SYMBOLS
#define _APS_NEXT_RESOURCE_VALUE        101
#define _APS_NEXT_COMMAND_VALUE         32771
#define _APS_NEXT_CONTROL_VALUE         1051
#define _APS_NEXT_SYMED_VALUE           101
#endif
#endif
