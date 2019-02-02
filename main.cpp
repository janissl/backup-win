#include <iostream>

#ifndef UNICODE
#define UNICODE
#endif

#ifndef _UNICODE
#define _UNICODE
#endif

#include <windows.h>
#include <tchar.h>
#include <shlwapi.h>
#include <strsafe.h>

FILE *log_stream;


void PrintUsage(const char *app_name) {
    std::cerr << std::endl;
    std::cerr << "USAGE: " << app_name << " SOURCE_DIRECTORY DESTINATION_DIRECTORY" << std::endl;
    std::cerr << std::endl;
    std::cerr << "\tSOURCE DIRECTORY:         The path of the directory to copy from" << std::endl;
    std::cerr << "\tDESTINATION DIRECTORY:    The path of the directory to copy to" << std::endl;
    std::cerr << std::endl;
}


bool GetLastWriteTime(LPCTSTR file_path, FILETIME *last_modified) {
    HANDLE hFile = CreateFile(file_path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);

    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }

    FILETIME ft_created, ft_accessed;
    int result = GetFileTime(hFile, &ft_created, &ft_accessed, last_modified);

    CloseHandle(hFile);
    return (result != 0);
}


bool IsBackupRequired(LPCTSTR source_path, LPCTSTR dest_path) {
    bool backup_required = false;
    if (!PathFileExists(dest_path)) {
        backup_required = true;
    } else {
        FILETIME source_time, dest_time;

        bool source_time_returned = GetLastWriteTime(source_path, &source_time);
        bool dest_time_returned = GetLastWriteTime(dest_path, &dest_time);

        if (source_time_returned && dest_time_returned) {
            const FILETIME* source_modified = &source_time;
            const FILETIME* dest_modified = &dest_time;

            if (CompareFileTime(source_modified, dest_modified) == 1) {
                backup_required = true;
            }
        } else {
            backup_required = true;
        }
    }

    return backup_required;
}


LPCTSTR GetErrorDescription(DWORD error_code) {
    LPCTSTR msg = nullptr;

    if (!FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_ALLOCATE_BUFFER,
                       nullptr,
                       error_code,
                       0,
                       (LPTSTR)&msg,
                       0,
                       nullptr)) {
        auto err_code = (unsigned int)GetLastError();

        LPCTSTR intro = _T("Format message failed with 0x");

        size_t error_code_max_digit_count = 5;
        auto int_str = new TCHAR[error_code_max_digit_count + 1];
        _itot_s((int)err_code, int_str, error_code_max_digit_count + 1, 16);

        size_t msg_len = _tcslen(intro) + _tcslen(int_str);

        auto err_msg = new TCHAR[msg_len + 1];
        StringCchCopy(err_msg, _tcslen(intro) + 1, intro);
        StringCchCat(err_msg, _tcslen(int_str) + 1, int_str);

        msg = err_msg;
    }

    return msg;
}


void BackupDirectory(LPCTSTR source_directory, LPCTSTR destination_directory) {
    auto *search_path = new TCHAR[MAX_PATH];
    StringCchCopy(search_path, _tcslen(source_directory) + 1, source_directory);
    LPCTSTR search_char = _T("*");
    PathAppend(search_path, search_char);

    WIN32_FIND_DATA fd;
    HANDLE hFind = ::FindFirstFile(search_path, &fd);

    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (StrCmp(fd.cFileName, _T(".")) == 0 || StrCmp(fd.cFileName, _T("..")) == 0)
                continue;

            auto *source_path = new TCHAR[MAX_PATH];
            StringCchCopy(source_path, _tcslen(source_directory) + 1, source_directory);
            PathAppend(source_path, fd.cFileName);

            auto *dest_path = new TCHAR[MAX_PATH];
            StringCchCopy(dest_path, _tcslen(destination_directory) + 1, destination_directory);
            PathAppend(dest_path, fd.cFileName);

            if (!PathFileExists(destination_directory)) {
                if (!CreateDirectory(destination_directory, nullptr)) {
                    LPCTSTR msg = GetErrorDescription(GetLastError());
                    _ftprintf(log_stream,
                              _T("FAILED to create '%ls' - %ls\n"),
                              destination_directory, msg);
                    continue;
                }
            }

            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                if (IsBackupRequired(source_path, dest_path)) {
                    if (!CopyFile(source_path, dest_path, false)) {
                        LPCTSTR msg = GetErrorDescription(GetLastError());
                        _ftprintf(log_stream,
                                  _T("FAILED to copy '%ls' to '%ls' - %ls\n"),
                                  source_path, dest_path, msg);
                    } else {
                        _ftprintf(log_stream,
                                  _T("'%ls' -> '%ls'\n"),
                                  source_path, dest_path);
                    }
                }
            } else {
                BackupDirectory(source_path, dest_path);
            }
        } while (::FindNextFile(hFind, &fd));

        ::FindClose(hFind);
    }
}


int main(int argc, char *argv[]) {
    if (argc != 3) {
        const char *exename = PathFindFileNameA(argv[0]);
        PrintUsage(exename);
        return 1;
    }

#ifdef UNICODE
    _wfopen_s(&log_stream, _T("last.log"), _T("w, ccs=UTF-8"));
#else
    fopen_s(&log_stream, "last.log_stream", "w");
#endif

    if (log_stream == nullptr) {
        std::cerr << "Failed to create a log_stream file!" << std::endl;
        return 1;
    }

    int src_dir_wc_len = MultiByteToWideChar(CP_UTF8, 0, argv[1], -1, nullptr, 0);
    auto *src_dir_wc = new TCHAR[src_dir_wc_len];
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, argv[1], -1, src_dir_wc, src_dir_wc_len);

    int dst_dir_wc_len = MultiByteToWideChar(CP_UTF8, 0, argv[2], -1, nullptr, 0);
    auto *dst_dir_wc = new TCHAR[dst_dir_wc_len];
    MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, argv[2], -1, dst_dir_wc, dst_dir_wc_len);

    if (!PathFileExists(src_dir_wc)) {
        _ftprintf(log_stream, _T("The source directory '%s' does not exist\n"), argv[1]);
        return 1;
    }

    BackupDirectory(src_dir_wc, dst_dir_wc);

    fclose(log_stream);
    return 0;
}
