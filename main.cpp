#include <iostream>
#include <memory>

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

using std::cerr;
using std::endl;
using std::unique_ptr;
using std::make_unique;


FILE *log_stream;


void PrintUsage(const char *app_name) {
    cerr << endl;
    cerr << "USAGE: " << app_name << " SOURCE_DIRECTORY DESTINATION_DIRECTORY" << endl;
    cerr << endl;
    cerr << "\tSOURCE DIRECTORY:         The path of the directory to copy from" << endl;
    cerr << "\tDESTINATION DIRECTORY:    The path of the directory to copy to" << endl;
    cerr << endl;
}


unique_ptr<TCHAR[]> CharPtrToTcharPtr(const char *input_string) {
    int output_string_len = MultiByteToWideChar(CP_ACP, 0, input_string, -1, nullptr, 0);
    auto output_string = make_unique<TCHAR[]>(static_cast<size_t>(output_string_len));
    MultiByteToWideChar(CP_ACP, MB_ERR_INVALID_CHARS, input_string, -1, output_string.get(), output_string_len);
    return output_string;
}


unique_ptr<TCHAR[]> JoinPath(LPCTSTR parent, LPCTSTR child) {
    auto new_path = make_unique<TCHAR[]>(MAX_PATH);
    StringCchCopy(new_path.get(), _tcslen(parent) + 1, parent);
    PathAppend(new_path.get(), child);
    return new_path;
}


bool GetLastWriteTime(LPCTSTR file_path, FILETIME *last_modified) {
    HANDLE hFile = CreateFile(file_path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);

    if (hFile == INVALID_HANDLE_VALUE) {
        return false;
    }

    FILETIME ft_created, ft_accessed;
    auto result = GetFileTime(hFile, &ft_created, &ft_accessed, last_modified);
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


unique_ptr<TCHAR[]> UIntToHexTcharPtr(size_t number) {
    size_t error_code_digit_count = GetDigitCount((size_t)number);
    auto int_str = make_unique<TCHAR[]>(error_code_digit_count + 1);
    _itot_s(static_cast<int>(number), int_str.get(), error_code_digit_count + 1, 16);
    return int_str;
}


unique_ptr<TCHAR[]> ConcatTcharPtr(LPCTSTR str1, LPCTSTR str2) {
    size_t out_len = _tcslen(str1) + _tcslen(str2);

    auto out_str = make_unique<TCHAR[]>(out_len + 1);
    StringCchCopy(out_str.get(), _tcslen(str1) + 1, str1);
    StringCchCat(out_str.get(), out_len + 1, str2);

    return out_str;
}


unique_ptr<TCHAR[]> BuildFormatErrorMessage(DWORD error_code) {
    auto intro = _T("Format message failed with 0x");
    auto int_str = UIntToHexTcharPtr((size_t)error_code);
    auto msg = ConcatTcharPtr(intro, int_str.get());
    return msg;
}


unique_ptr<TCHAR[]> GetErrorDescription(DWORD error_code) {
    unique_ptr<TCHAR[]> msg = nullptr;

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
    auto search_char = _T("*");
    auto search_path = JoinPath(source_directory, search_char);

    WIN32_FIND_DATA fd;
    HANDLE hFind = ::FindFirstFile(search_path.get(), &fd);

    if (hFind != INVALID_HANDLE_VALUE) {
        do {
            if (StrCmp(fd.cFileName, _T(".")) == 0 || StrCmp(fd.cFileName, _T("..")) == 0)
                continue;

            auto source_path = JoinPath(source_directory, fd.cFileName);
            auto dest_path = JoinPath(destination_directory, fd.cFileName);

            if (!PathFileExists(destination_directory)) {
                if (!CreateDirectory(destination_directory, nullptr)) {
                    auto msg = GetErrorDescription(GetLastError());
                    _ftprintf(log_stream,
                              _T("FAILED to create '%ls' - %ls\n"),
                              destination_directory, msg.get());
                    continue;
                }
            }

            if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
                if (IsBackupRequired(source_path.get(), dest_path.get())) {
                    if (!CopyFile(source_path.get(), dest_path.get(), false)) {
                        auto msg = GetErrorDescription(GetLastError());
                        _ftprintf(log_stream,
                                  _T("FAILED to copy '%ls' to '%ls' - %ls\n"),
                                  source_path.get(), dest_path.get(), msg.get());
                    } else {
                        _ftprintf(log_stream,
                                  _T("'%ls' -> '%ls'\n"),
                                  source_path.get(), dest_path.get());
                    }
                }
            } else {
                BackupDirectoryTree(source_path.get(), dest_path.get());
            }

        } while (::FindNextFile(hFind, &fd));

        ::FindClose(hFind);
    }
}


void RunBackup(const char *source_root, const char *dest_root) {
#ifdef UNICODE
    _wfopen_s(&log_stream, _T("last.log"), _T("w, ccs=UTF-8"));
#else
    fopen_s(&log_stream, "last.log", "w");
#endif

    if (log_stream == nullptr) {
        cerr << "Could not to create the last.log file!" << endl;
        return;
    }

    auto src_root = CharPtrToTcharPtr(source_root);
    auto dst_root = CharPtrToTcharPtr(dest_root);

    if (!PathFileExists(src_root.get())) {
        _ftprintf(log_stream, _T("The source directory '%s' does not exist\n"), source_root);
        return;
    }

    BackupDirectoryTree(src_root.get(), dst_root.get());

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
