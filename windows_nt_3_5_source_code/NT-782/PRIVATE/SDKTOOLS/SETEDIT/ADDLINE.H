void /*static*/ OnComputerChanged (HDLG hDlg) ;


BOOL AddLine (HWND hWndParent, 
              PPERFSYSTEM *ppSystemFirstView,
              PLINEVISUAL pLineVisual,
              int iLineTypeToAdd) ;


BOOL EditLine (HWND hWndParent,
               PPERFSYSTEM *ppSystemFirstView,
               PLINE pLineToEdit,
               int iLineTypeToEdit) ;
