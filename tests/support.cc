#if defined( _WIN32 )
#include <crtdbg.h>
#include <cstdlib>
#include <stdlib.h>

namespace
{
    const int quiet_crt = []
    {
        _set_error_mode( _OUT_TO_STDERR );
        _set_abort_behavior( 0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT );
        _CrtSetReportMode( _CRT_ASSERT, _CRTDBG_MODE_FILE );
        _CrtSetReportFile( _CRT_ASSERT, _CRTDBG_FILE_STDERR );
        _CrtSetReportMode( _CRT_ERROR, _CRTDBG_MODE_FILE );
        _CrtSetReportFile( _CRT_ERROR, _CRTDBG_FILE_STDERR );
        return 0;
    }( );
} // namespace
#endif
