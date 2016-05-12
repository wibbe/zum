
#include "nfd_common.h"

#include <wchar.h>
#include <stdio.h>
#include <assert.h>
#include <windows.h>

nfdresult_t NFD_OpenDialog( const nfdchar_t *filterList,
                            const nfdchar_t *defaultPath,
                            nfdchar_t **outPath )
{
  OPENFILENAME open;

  return NFD_ERROR;
}

/* multiple file open dialog */
nfdresult_t NFD_OpenDialogMultiple( const nfdchar_t *filterList,
                                    const nfdchar_t *defaultPath,
                                    nfdpathset_t *outPaths )
{
  return NFD_ERROR;
}

/* save dialog */
nfdresult_t NFD_SaveDialog( const nfdchar_t *filterList,
                            const nfdchar_t *defaultPath,
                            nfdchar_t **outPath )
{
  return NFD_ERROR;
}