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


LPCTSTR CharPtrToTcharPtr(const char *input_string) {
    int output_string_len = MultiByteToWideChar(CP_ACP, 0, input_string, -1, nullptr, 0);
    auto *output_string = new TCHAR[output_string_len];
    MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, input_string, -1, output_string, output_string_len);
    return output_string;
}


LPCTSTR JoinPath(LPCTSTR parent, LPCTSTR child) {
    auto *new_path = new TCHAR[MAX_PATH];
    StringCchCopy(new_path, _tcslen(parent) + 1, parent);
    PathAppend(new_path, child);
    return new_path;
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


size_t GetDigitCount(size_t number) {
    size_t digits = 0;

    while (number) {
        number /= 10;
        ++digits;
    }

    return digits;
}


LPCTSTR UIntToHexTcharPtr(size_t number) {
    size_t error_code_digit_count = GetDigitCount((size_t)number);
    auto int_str = new TCHAR[error_code_digit_count + 1];
    _itot_s((int)number, int_str, error_code_digit_count + 1, 16);
    return int_str;
}


LPCTSTR ConcatTcharPtr(LPCTSTR str1, LPCTSTR str2) {
    size_t out_len = _tcslen(str1) + _tcslen(str2);

    auto out_str = new TCHAR[out_len + 1];
    StringCchCopy(out_str, _tcslen(str1) + 1, str1);
    StringCchCat(out_str, out_len + 1, str2);

    return out_str;
}


LPCTSTR BuildFormatErrorMessage(DWORD error_code) {
    LPCTSTR intro = _T("Format message failed with 0x");
    LPCTSTR int_str = UIntToHexTcharPtr((size_t)error_code);
    LPCTSTR msg = ConcatTcharPtr(intro, int_str);
    delete [] int_str;
    return msg;
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
        msg = BuildFormatErrorMessage(GetLastError());
    }

    return msg;
}


void BackupDirectoryTree(LPCTSTR source_directory, LPCTSTR destination_directory) {
    LPCTSTR search_char = _T("*");
    LPCTSTR search_path = JoinPath(source_directory, search_char);

    WIN32_FIND_DATA fd;
    HANDLE hFind = ::FindFirstFile(search_path, &fd);

    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (StrCmp(fd.cFileName, _T(".")) == 0 || StrCmp(fd.cFileName, _T("..")) == 0)
                continue;

            LPCTSTR source_path = JoinPath(source_directory, fd.cFileName);
            LPCTSTR dest_path = JoinPath(destination_directory, fd.cFileName);

            if (!PathFileExists(destination_directory)) {
                if (!CreateDirectory(destination_directory, nullptr)) {
                    LPCTSTR msg = GetErrorDescription(GetLastError());
                    _ftprintf(log_stream,
                              _T("FAILED to create '%ls' - %ls\n"),
                              destination_directory, msg);
                    delete [] msg;
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
                        delete [] msg;
                    } else {
                        _ftprintf(log_stream,
                                  _T("'%ls' -> '%ls'\n"),
                                  source_path, dest_path);
                    }
                }
            } else {
                BackupDirectoryTree(source_path, dest_path);
            }

            delete [] source_path;
            delete [] dest_path;
        } while (::FindNextFile(hFind, &fd));

        ::FindClose(hFind);
    }

    delete [] search_path;
}


void RunBackup(const char *source_root, const char *dest_root) {
#ifdef UNICODE
    _wfopen_s(&log_stream, _T("last.log"), _T("w, ccs=UTF-8"));
#else
    fopen_s(&log_stream, "last.log_stream", "w");
#endif

    if (log_stream == nullptr) {
        std::cerr << "Could not to create the last.log file!" << std::endl;
        return;
    }

    LPCTSTR src_root = CharPtrToTcharPtr(source_root);
    LPCTSTR dst_root = CharPtrToTcharPtr(dest_root);

    if (!PathFileExists(src_root)) {
        _ftprintf(log_stream, _T("The source directory '%s' does not exist\n"), source_root);
        return;
    }

    BackupDirectoryTree(src_root, dst_root);

    delete [] src_root;
    delete [] dst_root;

    fclose(log_stream);
}


int main(int argc, char *argv[]) {
    if (argc != 3) {
        const char *exe_name = PathFindFileNameA(argv[0]);
        PrintUsage(exe_name);
        return 1;
    }

    RunBackup(argv[1], argv[2]);

    return 0;
}
