/*
 * mbed Microcontroller Library
 * Copyright (c) 2006-2016 ARM Limited
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

/* The following copyright notice is reproduced from the glibc project
 * REF_LICENCE_GLIBC
 *
 * Copyright (C) 1991, 1992 Free Software Foundation, Inc.
 * This file is part of the GNU C Library.
 *
 * The GNU C Library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * The GNU C Library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with the GNU C Library; see the file COPYING.LIB.  If
 * not, write to the Free Software Foundation, Inc., 675 Mass Ave,
 * Cambridge, MA 02139, USA.
 */


/** @file basic.cpp POSIX File API (stdio) test cases
 *
 * Consult the documentation under the test-case functions for
 * a description of the individual test case.
 *
 * this file includes ports for the mbed 2 test cases from the following locations:
 *  - https://github.com:/armmbed/mbed-os/features/unsupported/tests/mbed/dir_sd/main.cpp.
 *  - https://github.com:/armmbed/mbed-os/features/unsupported/tests/mbed/file/main.cpp.
 *  - https://github.com:/armmbed/mbed-os/features/unsupported/tests/mbed/sd/main.cpp
 *  - https://github.com:/armmbed/mbed-os/features/unsupported/tests/mbed/sd_perf_handle/main.cpp
 *  - https://github.com:/armmbed/mbed-os/features/unsupported/tests/mbed/sd_perf_stdio/main.cpp.
 */

#include "mbed.h"
#include "LittleFileSystem.h"
#include "test_env.h"
#include "fslittle_debug.h"
#include "fslittle_test.h"
#include "utest/utest.h"
#include "unity/unity.h"
#include "greentea-client/test_env.h"
#include "FlashIAP.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <algorithm>
/* retarget.h is included after errno.h so symbols are mapped to
 * consistent values for all toolchains */
#include "platform/mbed_retarget.h"

using namespace utest::v1;

/* DEVICE_SPI
 *  This symbol is defined in targets.json if the target has a SPI interface, which is required for SDCard support.
 *
 * MBED_CONF_APP_FSLITTLE_SDCARD_INSTALLED
 *  For testing purposes, an SDCard must be installed on the target for the test cases in this file to succeed.
 *  If the target has an SD card installed then the MBED_CONF_APP_FSLITTLE_SDCARD_INSTALLED will be generated
 *  from the mbed_app.json, which includes the line
 *    {
 *    "config": {
 *        "UART_RX": "D0",
 *        <<< lines removed >>>
 *        "DEVICE_SPI": 1,
 *        "MBED_CONF_APP_FSLITTLE_SDCARD_INSTALLED": 1
 *      },
 *      <<< lines removed >>>
 */

//#include "SDBlockDevice.h"
//SDBlockDevice sd(MBED_CONF_SD_SPI_MOSI, MBED_CONF_SD_SPI_MISO, MBED_CONF_SD_SPI_CLK, MBED_CONF_SD_SPI_CS);

#include "FlashIAPBlockDevice.h"
#include "SlicingBlockDevice.h"

FlashIAPBlockDevice *flash_bd;
SlicingBlockDevice *slice;
LittleFileSystem fs("sd");
bool abort_tests = false;

#define FSLITTLE_BASIC_TEST_        fslittle_basic_test_
#define FSLITTLE_BASIC_TEST_00      fslittle_basic_test_00
#define FSLITTLE_BASIC_TEST_01      fslittle_basic_test_01
#define FSLITTLE_BASIC_TEST_02      fslittle_basic_test_02
#define FSLITTLE_BASIC_TEST_03      fslittle_basic_test_03
#define FSLITTLE_BASIC_TEST_04      fslittle_basic_test_04
#define FSLITTLE_BASIC_TEST_05      fslittle_basic_test_05
#define FSLITTLE_BASIC_TEST_06      fslittle_basic_test_06
#define FSLITTLE_BASIC_TEST_07      fslittle_basic_test_07
#define FSLITTLE_BASIC_TEST_08      fslittle_basic_test_08
#define FSLITTLE_BASIC_TEST_09      fslittle_basic_test_09
#define FSLITTLE_BASIC_TEST_10      fslittle_basic_test_10

#define FSLITTLE_BASIC_MSG_BUF_SIZE              256
#define FSLITTLE_BASIC_TEST_05_TEST_STRING   "Hello World!"

static const char *sd_file_path = "/sd/out.txt";
static const int FSLITTLE_BASIC_DATA_SIZE = 256;
static char fslittle_basic_msg_g[FSLITTLE_BASIC_MSG_BUF_SIZE];
static char fslittle_basic_buffer[512];
static const int FSLITTLE_BASIC_KIB_RW = 16;
static const int MAX_TEST_SIZE = FSLITTLE_BASIC_KIB_RW * 1024 * 2;
static Timer fslittle_basic_timer;
static const char *fslittle_basic_bin_filename = "/sd/testfile.bin";
static const char *fslittle_basic_bin_filename_test_08 = "testfile.bin";
static const char *fslittle_basic_bin_filename_test_10 = "0:testfile.bin";

#define FSLITTLE_BASIC_MSG(_buf, _max_len, _fmt, ...)   \
  do                                                            \
  {                                                             \
      snprintf((_buf), (_max_len), (_fmt), __VA_ARGS__);        \
  }while(0);

// Align a value to a specified size.
// Parameters :
// val           - [IN]   Value.
// size          - [IN]   Size.
// Return        : Aligned value.
static inline uint32_t align_up(uint32_t val, uint32_t size)
{
    return (((val - 1) / size) + 1) * size;
}

/** @brief  test for operation of SDFileSystem::format()
 *
 * @return on success returns CaseNext to continue to next test case, otherwise will assert on errors.
 */
void fslittle_basic_test_()
{

    FSLITTLE_FENTRYLOG("%s:entered\n", __func__);
    int32_t ret = -1;

    FlashIAP flash;
    ret = flash.init();
    TEST_ASSERT_EQUAL(0, ret);

    uint32_t code_end_address = FLASHIAP_ROM_END - flash.get_flash_start();
    bd_addr_t code_end_offset = align_up(code_end_address, flash.get_sector_size(code_end_address));

    ret = flash.deinit();
    TEST_ASSERT_EQUAL(0, ret);

    flash_bd = new FlashIAPBlockDevice();
    ret = flash_bd->init();
    TEST_ASSERT_EQUAL(0, ret);

    // Use slice of last sectors
    bd_addr_t slice_addr = flash_bd->size();
    bd_size_t slice_size = 0;
    while (slice_size < MAX_TEST_SIZE) {
        bd_size_t unit_size = flash_bd->get_erase_size(slice_addr - 1);
        slice_addr -= unit_size;
        slice_size += unit_size;
    }

    if (slice_addr < code_end_offset) {
        abort_tests = true;
    }
    TEST_SKIP_UNLESS_MESSAGE(!abort_tests, "Test skipped. Test region overlaps code.");

    slice = new SlicingBlockDevice(flash_bd, slice_addr);
    slice->init();

    ret = fs.reformat(slice);
    FSLITTLE_TEST_UTEST_MESSAGE(fslittle_basic_msg_g, FSLITTLE_UTEST_MSG_BUF_SIZE,
                                "%s:Error: failed to format device (ret=%d)\n", __func__, (int) ret);
    TEST_ASSERT_MESSAGE(ret == 0, fslittle_basic_msg_g);
    fs.mount(slice);

}

/** @brief  fopen test case
 *
 * - open a file
 * - generate random data items, write the item to the file and store a coy in a buffer for later use.
 * - close the file.
 * - open the file.
 * - read the data items from the file and check they are the same as write.
 * - close the file.
 *
 * @return on success returns CaseNext to continue to next test case, otherwise will assert on errors.
 */
static void fslittle_basic_test_00()
{
    uint8_t data_written[FSLITTLE_BASIC_DATA_SIZE] = { 0 };
    bool read_result = false;
    bool write_result = false;

    TEST_SKIP_UNLESS_MESSAGE(!abort_tests, "Test skipped. Test region overlaps code.");

    int ret = fs.reformat(slice);
    FSLITTLE_TEST_UTEST_MESSAGE(fslittle_basic_msg_g, FSLITTLE_UTEST_MSG_BUF_SIZE,
                                "%s:Error: failed to format device (ret=%d)\n", __func__, (int) ret);
    TEST_ASSERT_MESSAGE(ret == 0, fslittle_basic_msg_g);

    // Fill data_written buffer with random data
    // Write these data into the file
    FSLITTLE_FENTRYLOG("%s:entered\n", __func__);
    {
        FSLITTLE_DBGLOG("%s:SD: Writing ... ", __func__);
        FILE *f = fopen(sd_file_path, "w");
        if (f) {
            for (int i = 0; i < FSLITTLE_BASIC_DATA_SIZE; i++) {
                data_written[i] = rand() % 0XFF;
                fprintf(f, "%c", data_written[i]);
            }
            write_result = true;
            fclose(f);
        }
        FSLITTLE_DBGLOG("[%s]\n", write_result ? "OK" : "FAIL");
    }
    TEST_ASSERT_MESSAGE(write_result == true, "Error: write_result is set to false.");

    // Read back the data from the file and store them in data_read
    {
        FSLITTLE_DBGLOG("%s:SD: Reading data ... ", __func__);
        FILE *f = fopen(sd_file_path, "r");
        if (f) {
            read_result = true;
            for (int i = 0; i < FSLITTLE_BASIC_DATA_SIZE; i++) {
                uint8_t data = fgetc(f);
                if (data != data_written[i]) {
                    read_result = false;
                    break;
                }
            }
            fclose(f);
        }
        FSLITTLE_DBGLOG("[%s]\n", read_result ? "OK" : "FAIL");
    }
    TEST_ASSERT_MESSAGE(read_result == true, "Error: read_result is set to false.");
}


/** @brief  test-fseek.c test ported from glibc project. See the licence at REF_LICENCE_GLIBC.
 *
 * @return on success returns CaseNext to continue to next test case, otherwise will assert on errors.
 */
static void fslittle_basic_test_01()
{
    FILE *fp, *fp1;
    int i, j;
    int ret = 0;

    FSLITTLE_FENTRYLOG("%s:entered\n", __func__);

    TEST_SKIP_UNLESS_MESSAGE(!abort_tests, "Test skipped. Test region overlaps code.");

    ret = fs.reformat(slice);
    FSLITTLE_TEST_UTEST_MESSAGE(fslittle_basic_msg_g, FSLITTLE_UTEST_MSG_BUF_SIZE,
                                "%s:Error: failed to format device (ret=%d)\n", __func__, (int) ret);
    TEST_ASSERT_MESSAGE(ret == 0, fslittle_basic_msg_g);

    fp = fopen (sd_file_path, "w+");
    if (fp == NULL) {
        FSLITTLE_DBGLOG("errno=%d\n", errno);
        TEST_ASSERT_MESSAGE(false, "error");
        return;
    }

    for (i = 0; i < 256; i++) {
        putc (i, fp);
    }
    /* FIXME: freopen() should open the specified file closing the first stream. As can be seen from the
     * code below, the old file descriptor fp can still be used, and this should not happen.
     */
    fp1 = freopen (sd_file_path, "r", fp);
    TEST_ASSERT_MESSAGE(fp1 == fp, "Error: cannot open file for reading");

    for (i = 1; i <= 255; i++) {
        ret = fseek (fp, (long) - i, SEEK_END);
        FSLITTLE_BASIC_MSG(fslittle_basic_msg_g, FSLITTLE_BASIC_MSG_BUF_SIZE, "%s:Error: fseek() failed (ret=%d).\n", __func__,
                           (int) ret);
        TEST_ASSERT_MESSAGE(ret == 0, fslittle_basic_msg_g);

        if ((j = getc (fp)) != 256 - i) {
            FSLITTLE_BASIC_MSG(fslittle_basic_msg_g, FSLITTLE_BASIC_MSG_BUF_SIZE, "%s: Error: SEEK_END failed (j=%d)\n",  __func__,
                               j);
            TEST_ASSERT_MESSAGE(false, fslittle_basic_msg_g);
        }
        ret = fseek (fp, (long) i, SEEK_SET);
        FSLITTLE_BASIC_MSG(fslittle_basic_msg_g, FSLITTLE_BASIC_MSG_BUF_SIZE, "%s: Error: Cannot SEEK_SET (ret=%d).\n",
                           __func__, (int) ret);
        TEST_ASSERT_MESSAGE(ret == 0, fslittle_basic_msg_g);

        if ((j = getc (fp)) != i) {
            FSLITTLE_BASIC_MSG(fslittle_basic_msg_g, FSLITTLE_BASIC_MSG_BUF_SIZE, "%s: Error: Cannot SEEK_SET (j=%d).\n", __func__,
                               j);
            TEST_ASSERT_MESSAGE(ret == 0, fslittle_basic_msg_g);
        }
        if ((ret = fseek (fp, (long) i, SEEK_SET))) {
            FSLITTLE_BASIC_MSG(fslittle_basic_msg_g, FSLITTLE_BASIC_MSG_BUF_SIZE, "%s: Error: Cannot SEEK_SET (ret=%d).\n",
                               __func__, (int) ret);
            TEST_ASSERT_MESSAGE(ret == 0, fslittle_basic_msg_g);
        }
        if ((ret = fseek (fp, (long) (i >= 128 ? -128 : 128), SEEK_CUR))) {
            FSLITTLE_BASIC_MSG(fslittle_basic_msg_g, FSLITTLE_BASIC_MSG_BUF_SIZE, "%s: Error: Cannot SEEK_CUR (ret=%d).\n",
                               __func__, (int) ret);
            TEST_ASSERT_MESSAGE(ret == 0, fslittle_basic_msg_g);
        }
        if ((j = getc (fp)) != (i >= 128 ? i - 128 : i + 128)) {
            FSLITTLE_BASIC_MSG(fslittle_basic_msg_g, FSLITTLE_BASIC_MSG_BUF_SIZE, "%s: Error: Cannot SEEK_CUR (j=%d).\n", __func__,
                               j);
            TEST_ASSERT_MESSAGE(ret == 0, fslittle_basic_msg_g);
        }
    }
    fclose (fp);
    remove(sd_file_path);
}


/** @brief  test_rdwr.c test ported from glibc project. See the licence at REF_LICENCE_GLIBC.
 *
 * WARNING: this test does not currently work. See WARNING comments below.
 *
 * @return on success returns CaseNext to continue to next test case, otherwise will assert on errors.
 */
static void fslittle_basic_test_02()
{
    static const char hello[] = "Hello, world.\n";
    static const char replace[] = "Hewwo, world.\n";
    static const size_t replace_from = 2, replace_to = 4;
    const char *filename = sd_file_path;
    char buf[BUFSIZ];
    FILE *f;
    int lose = 0;
    int32_t ret = 0;
    char *rets = NULL;

    FSLITTLE_FENTRYLOG("%s:entered\n", __func__);

    TEST_SKIP_UNLESS_MESSAGE(!abort_tests, "Test skipped. Test region overlaps code.");


    ret = fs.reformat(slice);
    FSLITTLE_TEST_UTEST_MESSAGE(fslittle_basic_msg_g, FSLITTLE_UTEST_MSG_BUF_SIZE,
                                "%s:Error: failed to format device (ret=%d)\n", __func__, (int) ret);
    TEST_ASSERT_MESSAGE(ret == 0, fslittle_basic_msg_g)

    f = fopen(filename, "w+");
    if (f == NULL) {
        FSLITTLE_BASIC_MSG(fslittle_basic_msg_g, FSLITTLE_BASIC_MSG_BUF_SIZE,
                           "%s: Error: Cannot open file for writing (filename=%s).\n", __func__, filename);
        TEST_ASSERT_MESSAGE(false, fslittle_basic_msg_g);
    }

    ret = fputs(hello, f);
    if (ret == EOF) {
        FSLITTLE_BASIC_MSG(fslittle_basic_msg_g, FSLITTLE_BASIC_MSG_BUF_SIZE,
                           "%s: Error: fputs() failed to write string to file (filename=%s, string=%s).\n", __func__, filename, hello);
        TEST_ASSERT_MESSAGE(false, fslittle_basic_msg_g);
    }

    rewind(f);
    rets = fgets(buf, sizeof(buf), f);
    if (rets == NULL) {
        FSLITTLE_BASIC_MSG(fslittle_basic_msg_g, FSLITTLE_BASIC_MSG_BUF_SIZE,
                           "%s: Error: fgets() failed to get string from file (filename=%s).\n", __func__, filename);
        TEST_ASSERT_MESSAGE(false, fslittle_basic_msg_g);
    }
    rets = NULL;

    rewind(f);
    ret = fputs(buf, f);
    if (ret == EOF) {
        FSLITTLE_BASIC_MSG(fslittle_basic_msg_g, FSLITTLE_BASIC_MSG_BUF_SIZE,
                           "%s: Error: fputs() failed to write string to file (filename=%s, string=%s).\n", __func__, filename, buf);
        TEST_ASSERT_MESSAGE(false, fslittle_basic_msg_g);
    }

    rewind(f);
    {
        register size_t i;
        for (i = 0; i < replace_from; ++i) {
            int c = getc(f);
            if (c == EOF) {
                FSLITTLE_DBGLOG("EOF at %u.\n", i);
                lose = 1;
                break;
            } else if (c != hello[i]) {
                FSLITTLE_DBGLOG("Got '%c' instead of '%c' at %u.\n",
                                (unsigned char) c, hello[i], i);
                lose = 1;
                break;
            }
        }
    }
    /* WARNING: printf("%s: here1. (lose = %d)\n", __func__, lose); */
    {
        long int where = ftell(f);
        if (where == replace_from) {
            register size_t i;
            for (i = replace_from; i < replace_to; ++i) {
                if (putc(replace[i], f) == EOF) {
                    FSLITTLE_DBGLOG("putc('%c') got %s at %u.\n",
                                    replace[i], strerror(errno), i);
                    lose = 1;
                    break;
                }
                /* WARNING: The problem seems to be that putc() is not writing the 'w' chars into the file
                 * FSLITTLE_DBGLOG("%s: here1.5. (char = %c, char as int=%d, ret=%d) \n", __func__, replace[i], (int) replace[i], ret);
                 */
            }
        } else if (where == -1L) {
            FSLITTLE_DBGLOG("ftell got %s (should be at %u).\n",
                            strerror(errno), replace_from);
            lose = 1;
        } else {
            FSLITTLE_DBGLOG("ftell returns %ld; should be %u.\n", where, replace_from);
            lose = 1;
        }
    }

    if (!lose) {
        rewind(f);
        memset(buf, 0, BUFSIZ);
        if (fgets(buf, sizeof(buf), f) == NULL) {
            FSLITTLE_DBGLOG("fgets got %s.\n", strerror(errno));
            lose = 1;
        } else if (strcmp(buf, replace)) {
            FSLITTLE_DBGLOG("Read \"%s\" instead of \"%s\".\n", buf, replace);
            lose = 1;
        }
    }

    if (lose) {
        FSLITTLE_BASIC_MSG(fslittle_basic_msg_g, FSLITTLE_BASIC_MSG_BUF_SIZE,
                           "%s: Error: Test Failed. Losing file (filename=%s).\n", __func__, filename);
        TEST_ASSERT_MESSAGE(false, fslittle_basic_msg_g);
    }
    remove(filename);
}

/** @brief  temptest.c test ported from glibc project. See the licence at REF_LICENCE_GLIBC.
 *
 * tmpnam() is currently not implemented
 *
 * @return on success returns CaseNext to continue to next test case, otherwise will assert on errors.
 */
static void fslittle_basic_test_03()
{
    char *fn = NULL;

    FSLITTLE_FENTRYLOG("%s:entered\n", __func__);

    TEST_SKIP_UNLESS_MESSAGE(!abort_tests, "Test skipped. Test region overlaps code.");

    fn = tmpnam((char *) NULL);
    FSLITTLE_BASIC_MSG(fslittle_basic_msg_g, FSLITTLE_BASIC_MSG_BUF_SIZE,
                       "%s: Error: appeared to generate a filename when function is not implemented.\n", __func__);
    TEST_ASSERT_MESSAGE(fn == NULL, fslittle_basic_msg_g);
}


static bool fslittle_basic_fileno_check(const char *name, FILE *stream, int fd)
{
    /* ARMCC stdio.h currently does not define fileno() */
#ifndef __ARMCC_VERSION
    int sfd = fileno (stream);
    FSLITTLE_DBGLOG("(fileno (%s) = %d) %c= %d\n", name, sfd, sfd == fd ? '=' : '!', fd);

    if (sfd == fd) {
        return true;
    } else {
        return false;
    }
#else
    /* For ARMCC behave as though test had passed. */
    return true;
#endif  /* __ARMCC_VERSION */
}

/* defines for next test case */
#ifndef STDIN_FILENO
#define STDIN_FILENO     0
#endif

#ifndef STDOUT_FILENO
#define STDOUT_FILENO    1
#endif

#ifndef STDERR_FILENO
#define STDERR_FILENO    2
#endif


/** @brief  tst-fileno.c test ported from glibc project. See the licence at REF_LICENCE_GLIBC.
 *
 * WARNING: this test does not currently work. See WARNING comments below.
 *
 *
 * @return on success returns CaseNext to continue to next test case, otherwise will assert on errors.
 */
static void fslittle_basic_test_04()
{
    /* ARMCC stdio.h currently does not define fileno() */
#ifndef __ARMCC_VERSION

    TEST_SKIP_UNLESS_MESSAGE(!abort_tests, "Test skipped. Test region overlaps code.");

    int ret = -1;
    ret = fslittle_basic_fileno_check("stdin", stdin, STDIN_FILENO);
    FSLITTLE_BASIC_MSG(fslittle_basic_msg_g, FSLITTLE_BASIC_MSG_BUF_SIZE,
                       "%s: Error: stdin does not have expected file number (expected=%d, fileno=%d.\n", __func__, (int) stdin, fileno(stdin));
    TEST_ASSERT_MESSAGE(ret == true, fslittle_basic_msg_g);

    ret = fslittle_basic_fileno_check("stdout", stdout, STDOUT_FILENO);
    FSLITTLE_BASIC_MSG(fslittle_basic_msg_g, FSLITTLE_BASIC_MSG_BUF_SIZE,
                       "%s: Error: stdout does not have expected file number (expected=%d, fileno=%d.\n", __func__, (int) stdout,
                       fileno(stdout));
    TEST_ASSERT_MESSAGE(ret == true, fslittle_basic_msg_g);

    ret = fslittle_basic_fileno_check("stderr", stderr, STDERR_FILENO);
    FSLITTLE_BASIC_MSG(fslittle_basic_msg_g, FSLITTLE_BASIC_MSG_BUF_SIZE,
                       "%s: Error: stderr does not have expected file number (expected=%d, fileno=%d.\n", __func__, (int) stderr,
                       fileno(stderr));
    TEST_ASSERT_MESSAGE(ret == true, fslittle_basic_msg_g);
#endif  /* __ARMCC_VERSION */
}


/** @brief  basic test to opendir() on a directory.
 *
 * This test has been ported from armmbed/mbed-os/features/unsupported/tests/mbed/dir_sd/main.cpp.
 *
 * @return on success returns CaseNext to continue to next test case, otherwise will assert on errors.
 */
static void fslittle_basic_test_05()
{
    FILE *f;
    const char *str = FSLITTLE_BASIC_TEST_05_TEST_STRING;
    int ret = 0;

    TEST_SKIP_UNLESS_MESSAGE(!abort_tests, "Test skipped. Test region overlaps code.");

    ret = fs.reformat(slice);
    FSLITTLE_TEST_UTEST_MESSAGE(fslittle_basic_msg_g, FSLITTLE_UTEST_MSG_BUF_SIZE,
                                "%s:Error: failed to format device (ret=%d)\n", __func__, (int) ret);
    TEST_ASSERT_MESSAGE(ret == 0, fslittle_basic_msg_g);

    FSLITTLE_DBGLOG("%s:Write files\n", __func__);
    char filename[32];
    for (int i = 0; i < 10; i++) {
        sprintf(filename, "/sd/test_%d.txt", i);
        FSLITTLE_DBGLOG("Creating file: %s\n", filename);

        f = fopen(filename, "w");
        FSLITTLE_BASIC_MSG(fslittle_basic_msg_g, FSLITTLE_BASIC_MSG_BUF_SIZE, "%s: Error: fopen() failed.\n", __func__);
        TEST_ASSERT_MESSAGE(f != NULL, fslittle_basic_msg_g);

        ret = fprintf(f, str);
        FSLITTLE_BASIC_MSG(fslittle_basic_msg_g, FSLITTLE_BASIC_MSG_BUF_SIZE, "%s: Error: writing file.\n", __func__);
        TEST_ASSERT_MESSAGE(ret == (int) strlen(str), fslittle_basic_msg_g);

        ret = fclose(f);
        FSLITTLE_BASIC_MSG(fslittle_basic_msg_g, FSLITTLE_BASIC_MSG_BUF_SIZE, "%s: Error: fclose() failed.\n", __func__);
        TEST_ASSERT_MESSAGE(ret == 0, fslittle_basic_msg_g);

        ret = remove(filename);
        FSLITTLE_BASIC_MSG(fslittle_basic_msg_g, FSLITTLE_BASIC_MSG_BUF_SIZE, "%s: Error: remove() failed.\n", __func__);
        TEST_ASSERT_MESSAGE(ret == 0, fslittle_basic_msg_g);
    }

    FSLITTLE_DBGLOG("%s:List files:\n", __func__);
    DIR *d = opendir("/sd");
    FSLITTLE_BASIC_MSG(fslittle_basic_msg_g, FSLITTLE_BASIC_MSG_BUF_SIZE, "%s: Error: opendir() failed.\n", __func__);
    TEST_ASSERT_MESSAGE(d != NULL, fslittle_basic_msg_g);

    struct dirent *p;
    while ((p = readdir(d)) != NULL) {
        FSLITTLE_DBGLOG("%s\n", p->d_name);
    }
    closedir(d);

}


/** @brief  basic test to write a file to sd card, and read it back again
 *
 * This test has been ported from armmbed/mbed-os/features/unsupported/tests/mbed/file/main.cpp.
 *
 * @return on success returns CaseNext to continue to next test case, otherwise will assert on errors.
 */
static void fslittle_basic_test_06()
{
    int ret = -1;
    char mac[16];

    TEST_SKIP_UNLESS_MESSAGE(!abort_tests, "Test skipped. Test region overlaps code.");

    mbed_mac_address(mac);
    FSLITTLE_DBGLOG("mac address: %02x,%02x,%02x,%02x,%02x,%02x\n", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

    ret = fs.reformat(slice);
    FSLITTLE_TEST_UTEST_MESSAGE(fslittle_basic_msg_g, FSLITTLE_UTEST_MSG_BUF_SIZE,
                                "%s:Error: failed to format device (ret=%d)\n", __func__, (int) ret);
    TEST_ASSERT_MESSAGE(ret == 0, fslittle_basic_msg_g);

    FILE *f;
    const char *str = FSLITTLE_BASIC_TEST_05_TEST_STRING;
    int str_len = strlen(FSLITTLE_BASIC_TEST_05_TEST_STRING);

    f = fopen(sd_file_path, "w");
    FSLITTLE_BASIC_MSG(fslittle_basic_msg_g, FSLITTLE_BASIC_MSG_BUF_SIZE, "%s: Error: fopen() failed.\n", __func__);
    TEST_ASSERT_MESSAGE(f != NULL, fslittle_basic_msg_g);

    ret = fprintf(f, str);
    FSLITTLE_BASIC_MSG(fslittle_basic_msg_g, FSLITTLE_BASIC_MSG_BUF_SIZE, "%s: Error: writing file.\n", __func__);
    TEST_ASSERT_MESSAGE(ret == (int) strlen(str), fslittle_basic_msg_g);

    ret = fclose(f);
    FSLITTLE_BASIC_MSG(fslittle_basic_msg_g, FSLITTLE_BASIC_MSG_BUF_SIZE, "%s: Error: fclose() failed.\n", __func__);
    TEST_ASSERT_MESSAGE(ret == 0, fslittle_basic_msg_g);

    // Read
    f = fopen(sd_file_path, "r");
    FSLITTLE_BASIC_MSG(fslittle_basic_msg_g, FSLITTLE_BASIC_MSG_BUF_SIZE, "%s: Error: fopen() failed.\n", __func__);
    TEST_ASSERT_MESSAGE(f != NULL, fslittle_basic_msg_g);

    int n = fread(fslittle_basic_buffer, sizeof(unsigned char), str_len, f);
    FSLITTLE_BASIC_MSG(fslittle_basic_msg_g, FSLITTLE_BASIC_MSG_BUF_SIZE, "%s: Error: fread() failed.\n", __func__);
    TEST_ASSERT_MESSAGE(n == str_len, fslittle_basic_msg_g);

    ret = fclose(f);
    FSLITTLE_BASIC_MSG(fslittle_basic_msg_g, FSLITTLE_BASIC_MSG_BUF_SIZE, "%s: Error: fclose() failed.\n", __func__);
    TEST_ASSERT_MESSAGE(ret == 0, fslittle_basic_msg_g);

}


/** @brief  basic test to write a file to sd card.
 *
 * This test has been ported from armmbed/mbed-os/features/unsupported/tests/mbed/sd/main.cpp.
 *
 * @return on success returns CaseNext to continue to next test case, otherwise will assert on errors.
 */
static void fslittle_basic_test_07()
{
    uint8_t data_written[FSLITTLE_BASIC_DATA_SIZE] = { 0 };

    TEST_SKIP_UNLESS_MESSAGE(!abort_tests, "Test skipped. Test region overlaps code.");

    int ret = fs.reformat(slice);
    FSLITTLE_TEST_UTEST_MESSAGE(fslittle_basic_msg_g, FSLITTLE_UTEST_MSG_BUF_SIZE,
                                "%s:Error: failed to format device (ret=%d)\n", __func__, (int) ret);
    TEST_ASSERT_MESSAGE(ret == 0, fslittle_basic_msg_g);

    // Fill data_written buffer with random data
    // Write these data into the file
    bool write_result = false;
    {
        FSLITTLE_DBGLOG("%s:SD: Writing ... ", __func__);
        FILE *f = fopen(sd_file_path, "w");
        if (f) {
            for (int i = 0; i < FSLITTLE_BASIC_DATA_SIZE; i++) {
                data_written[i] = rand() % 0XFF;
                fprintf(f, "%c", data_written[i]);
            }
            write_result = true;
            fclose(f);
        }
        FSLITTLE_DBGLOG("[%s]\n", write_result ? "OK" : "FAIL");
        FSLITTLE_BASIC_MSG(fslittle_basic_msg_g, FSLITTLE_BASIC_MSG_BUF_SIZE, "%s: Error: unexpected write failure.\n",
                           __func__);
        TEST_ASSERT_MESSAGE(write_result == true, fslittle_basic_msg_g);
    }

    // Read back the data from the file and store them in data_read
    bool read_result = false;
    {
        FSLITTLE_DBGLOG("%s:SD: Reading data ... ", __func__);
        FILE *f = fopen(sd_file_path, "r");
        if (f) {
            read_result = true;
            for (int i = 0; i < FSLITTLE_BASIC_DATA_SIZE; i++) {
                uint8_t data = fgetc(f);
                if (data != data_written[i]) {
                    read_result = false;
                    break;
                }
            }
            fclose(f);
        }
        FSLITTLE_DBGLOG("[%s]\n", read_result ? "OK" : "FAIL");
        FSLITTLE_BASIC_MSG(fslittle_basic_msg_g, FSLITTLE_BASIC_MSG_BUF_SIZE, "%s: Error: unexpected read failure.\n",
                           __func__);
        TEST_ASSERT_MESSAGE(read_result == true, fslittle_basic_msg_g);
    }
}


static bool fslittle_basic_test_file_write_fhandle(const char *filename, const int kib_rw)
{
    int ret = -1;
    File file;

    ret = file.open(&fs, filename, O_WRONLY | O_CREAT | O_TRUNC);
    FSLITTLE_BASIC_MSG(fslittle_basic_msg_g, FSLITTLE_BASIC_MSG_BUF_SIZE, "%s: Error: failed to open file.\n", __func__);
    TEST_ASSERT_MESSAGE(ret == 0, fslittle_basic_msg_g);

    int byte_write = 0;
    fslittle_basic_timer.start();
    for (int i = 0; i < kib_rw; i++) {
        ret = file.write(fslittle_basic_buffer, sizeof(fslittle_basic_buffer));
        FSLITTLE_BASIC_MSG(fslittle_basic_msg_g, FSLITTLE_BASIC_MSG_BUF_SIZE, "%s: Error: failed to write to file.\n",
                           __func__);
        TEST_ASSERT_MESSAGE(ret == sizeof(fslittle_basic_buffer), fslittle_basic_msg_g);
        byte_write++;
    }
    fslittle_basic_timer.stop();
    file.close();
#ifdef FSLITTLE_DEBUG
    double test_time_sec = fslittle_basic_timer.read_us() / 1000000.0;
    double speed = kib_rw / test_time_sec;
    FSLITTLE_DBGLOG("%d KiB write in %.3f sec with speed of %.4f KiB/s\n", byte_write, test_time_sec, speed);
#endif
    fslittle_basic_timer.reset();
    return true;
}


static bool fslittle_basic_test_file_read_fhandle(const char *filename, const int kib_rw)
{
    int ret = -1;
    File file;
    ret = file.open(&fs, filename, O_RDONLY);

    FSLITTLE_BASIC_MSG(fslittle_basic_msg_g, FSLITTLE_BASIC_MSG_BUF_SIZE, "%s: Error: failed to open file.\n", __func__);
    TEST_ASSERT_MESSAGE(ret == 0, fslittle_basic_msg_g);

    fslittle_basic_timer.start();
    int byte_read = 0;
    while (file.read(fslittle_basic_buffer, sizeof(fslittle_basic_buffer)) == sizeof(fslittle_basic_buffer)) {
        byte_read++;
    }
    fslittle_basic_timer.stop();
    file.close();
#ifdef FSLITTLE_DEBUG
    double test_time_sec = fslittle_basic_timer.read_us() / 1000000.0;
    double speed = kib_rw / test_time_sec;
    FSLITTLE_DBGLOG("%d KiB read in %.3f sec with speed of %.4f KiB/s\n", byte_read, test_time_sec, speed);
#endif
    fslittle_basic_timer.reset();
    return true;
}


static char fslittle_basic_test_random_char()
{
    return rand() % 100;
}


/** @brief  basic sd card performance test
 *
 * This test has been ported from armmbed/mbed-os/features/unsupported/tests/mbed/sd_perf_handle/main.cpp.
 *
 * @return on success returns CaseNext to continue to next test case, otherwise will assert on errors.
 */
static void fslittle_basic_test_08()
{
    TEST_SKIP_UNLESS_MESSAGE(!abort_tests, "Test skipped. Test region overlaps code.");

    // Test header
    FSLITTLE_DBGLOG("\n%s:SD Card FileHandle Performance Test\n", __func__);
    FSLITTLE_DBGLOG("File name: %s\n", fslittle_basic_bin_filename);
    FSLITTLE_DBGLOG("Buffer size: %d KiB\n", (FSLITTLE_BASIC_KIB_RW * sizeof(fslittle_basic_buffer)) / 1024);

    int ret = fs.reformat(slice);
    FSLITTLE_TEST_UTEST_MESSAGE(fslittle_basic_msg_g, FSLITTLE_UTEST_MSG_BUF_SIZE,
                                "%s:Error: failed to format device (ret=%d)\n", __func__, (int) ret);
    TEST_ASSERT_MESSAGE(ret == 0, fslittle_basic_msg_g);

    // Initialize buffer
    srand(0);
    char *buffer_end = fslittle_basic_buffer + sizeof(fslittle_basic_buffer);
    std::generate (fslittle_basic_buffer, buffer_end, fslittle_basic_test_random_char);

    bool result = true;
    for (;;) {
        FSLITTLE_DBGLOG("%s:Write test...\n", __func__);
        if (fslittle_basic_test_file_write_fhandle(fslittle_basic_bin_filename_test_08, FSLITTLE_BASIC_KIB_RW) == false) {
            result = false;
            break;
        }

        FSLITTLE_DBGLOG("%s:Read test...\n", __func__);
        if (fslittle_basic_test_file_read_fhandle(fslittle_basic_bin_filename_test_08, FSLITTLE_BASIC_KIB_RW) == false) {
            result = false;
            break;
        }
        break;
    }
    TEST_ASSERT_MESSAGE(result == true, "something went wrong");
}


bool fslittle_basic_test_sf_file_write_stdio(const char *filename, const int kib_rw)
{
    int ret = -1;
    FILE *file = fopen(filename, "w");

    FSLITTLE_BASIC_MSG(fslittle_basic_msg_g, FSLITTLE_BASIC_MSG_BUF_SIZE, "%s: Error: failed to open file.\n", __func__);
    TEST_ASSERT_MESSAGE(file != NULL, fslittle_basic_msg_g);

    int byte_write = 0;
    fslittle_basic_timer.start();
    for (int i = 0; i < kib_rw; i++) {
        ret = fwrite(fslittle_basic_buffer, sizeof(char), sizeof(fslittle_basic_buffer), file);
        FSLITTLE_BASIC_MSG(fslittle_basic_msg_g, FSLITTLE_BASIC_MSG_BUF_SIZE, "%s: Error: failed to write to file.\n",
                           __func__);
        TEST_ASSERT_MESSAGE(ret == sizeof(fslittle_basic_buffer), fslittle_basic_msg_g);
        byte_write++;
    }
    fslittle_basic_timer.stop();
    fclose(file);
#ifdef FSLITTLE_DEBUG
    double test_time_sec = fslittle_basic_timer.read_us() / 1000000.0;
    double speed = kib_rw / test_time_sec;
    FSLITTLE_DBGLOG("%d KiB write in %.3f sec with speed of %.4f KiB/s\n", byte_write, test_time_sec, speed);
#endif
    fslittle_basic_timer.reset();
    return true;
}


bool fslittle_basic_test_sf_file_read_stdio(const char *filename, const int kib_rw)
{
    FILE *file = fopen(filename, "r");

    FSLITTLE_BASIC_MSG(fslittle_basic_msg_g, FSLITTLE_BASIC_MSG_BUF_SIZE, "%s: Error: failed to open file.\n", __func__);
    TEST_ASSERT_MESSAGE(file != NULL, fslittle_basic_msg_g);
    fslittle_basic_timer.start();
    int byte_read = 0;
    while (fread(fslittle_basic_buffer, sizeof(char), sizeof(fslittle_basic_buffer),
                 file) == sizeof(fslittle_basic_buffer)) {
        byte_read++;
    }
    fslittle_basic_timer.stop();
    fclose(file);
#ifdef FSLITTLE_DEBUG
    double test_time_sec = fslittle_basic_timer.read_us() / 1000000.0;
    double speed = kib_rw / test_time_sec;
    FSLITTLE_DBGLOG("%d KiB read in %.3f sec with speed of %.4f KiB/s\n", byte_read, test_time_sec, speed);
#endif
    fslittle_basic_timer.reset();
    return true;
}


/** @brief  basic test to write a file to sd card.
 *
 * This test has been ported from armmbed/mbed-os/features/unsupported/tests/mbed/sd_perf_stdio/main.cpp.
 *
 * @return on success returns CaseNext to continue to next test case, otherwise will assert on errors.
 */
static void fslittle_basic_test_09()
{
    TEST_SKIP_UNLESS_MESSAGE(!abort_tests, "Test skipped. Test region overlaps code.");

    // Test header
    FSLITTLE_DBGLOG("\n%s:SD Card Stdio Performance Test\n", __func__);
    FSLITTLE_DBGLOG("File name: %s\n", fslittle_basic_bin_filename);
    FSLITTLE_DBGLOG("Buffer size: %d KiB\n", (FSLITTLE_BASIC_KIB_RW * sizeof(fslittle_basic_buffer)) / 1024);

    int ret = fs.reformat(slice);
    FSLITTLE_TEST_UTEST_MESSAGE(fslittle_basic_msg_g, FSLITTLE_UTEST_MSG_BUF_SIZE,
                                "%s:Error: failed to format device (ret=%d)\n", __func__, (int) ret);
    TEST_ASSERT_MESSAGE(ret == 0, fslittle_basic_msg_g);

    // Initialize buffer
    srand(0);
    char *buffer_end = fslittle_basic_buffer + sizeof(fslittle_basic_buffer);
    std::generate (fslittle_basic_buffer, buffer_end, fslittle_basic_test_random_char);

    bool result = true;
    for (;;) {
        FSLITTLE_DBGLOG("%s:Write test...\n", __func__);
        if (fslittle_basic_test_sf_file_write_stdio(fslittle_basic_bin_filename, FSLITTLE_BASIC_KIB_RW) == false) {
            result = false;
            break;
        }

        FSLITTLE_DBGLOG("%s:Read test...\n", __func__);
        if (fslittle_basic_test_sf_file_read_stdio(fslittle_basic_bin_filename, FSLITTLE_BASIC_KIB_RW) == false) {
            result = false;
            break;
        }
        break;
    }
    TEST_ASSERT_MESSAGE(result == true, "Expected true result not found");
}


bool fslittle_basic_test_file_write_littlefs(const char *filename, const int kib_rw)
{
    File file;
    int res = file.open(&fs, filename, O_CREAT | O_WRONLY);

    FSLITTLE_BASIC_MSG(fslittle_basic_msg_g, FSLITTLE_BASIC_MSG_BUF_SIZE, "%s: Error: failed to open file.\n", __func__);
    TEST_ASSERT_MESSAGE(res == 0, fslittle_basic_msg_g);

    int byte_write = 0;
    unsigned int bytes = 0;
    fslittle_basic_timer.start();
    for (int i = 0; i < kib_rw; i++) {
        bytes = file.write(fslittle_basic_buffer, sizeof(fslittle_basic_buffer));
        FSLITTLE_BASIC_MSG(fslittle_basic_msg_g, FSLITTLE_BASIC_MSG_BUF_SIZE, "%s: Error: failed to write to file.\n",
                           __func__);
        TEST_ASSERT_MESSAGE(bytes > 0, fslittle_basic_msg_g);
        byte_write++;
    }
    fslittle_basic_timer.stop();
    file.close();
#ifdef FSLITTLE_DEBUG
    double test_time_sec = fslittle_basic_timer.read_us() / 1000000.0;
    double speed = kib_rw / test_time_sec;
    FSLITTLE_DBGLOG("%d KiB write in %.3f sec with speed of %.4f KiB/s\n", byte_write, test_time_sec, speed);
#endif
    fslittle_basic_timer.reset();
    return true;
}

bool fslittle_basic_test_file_read_littlefs(const char *filename, const int kib_rw)
{
    File file;
    int res = file.open(&fs, filename, O_RDONLY);

    FSLITTLE_BASIC_MSG(fslittle_basic_msg_g, FSLITTLE_BASIC_MSG_BUF_SIZE, "%s: Error: failed to open file.\n", __func__);
    TEST_ASSERT_MESSAGE(res == 0, fslittle_basic_msg_g);

    fslittle_basic_timer.start();
    int byte_read = 0;
    unsigned int bytes = 0;
    do {
        bytes = file.read(fslittle_basic_buffer, sizeof(fslittle_basic_buffer));
        byte_read++;
    } while (res == 0 && bytes == sizeof(fslittle_basic_buffer));
    fslittle_basic_timer.stop();
    file.close();
#ifdef FSLITTLE_DEBUG
    double test_time_sec = fslittle_basic_timer.read_us() / 1000000.0;
    double speed = kib_rw / test_time_sec;
    FSLITTLE_DBGLOG("%d KiB read in %.3f sec with speed of %.4f KiB/s\n", byte_read, test_time_sec, speed);
#endif
    fslittle_basic_timer.reset();
    return true;
}

/** @brief  basic test to write a file to sd card.
 *
 * This test has been ported from armmbed/mbed-os/features/unsupported/tests/mbed/sd_perf_stdio/main.cpp.
 *
 * @return on success returns CaseNext to continue to next test case, otherwise will assert on errors.
 */
static void fslittle_basic_test_10()
{
    TEST_SKIP_UNLESS_MESSAGE(!abort_tests, "Test skipped. Test region overlaps code.");

    // Test header
    FSLITTLE_DBGLOG("\n%sSD Card FatFS Performance Test\n", __func__);
    FSLITTLE_DBGLOG("File name: %s\n", fslittle_basic_bin_filename_test_10);
    FSLITTLE_DBGLOG("Buffer size: %d KiB\n", (FSLITTLE_BASIC_KIB_RW * sizeof(fslittle_basic_buffer)) / 1024);

    int ret = fs.reformat(slice);
    FSLITTLE_TEST_UTEST_MESSAGE(fslittle_basic_msg_g, FSLITTLE_UTEST_MSG_BUF_SIZE,
                                "%s:Error: failed to format device (ret=%d)\n", __func__, (int) ret);
    TEST_ASSERT_MESSAGE(ret == 0, fslittle_basic_msg_g);

    // Initialize buffer
    srand(1);
    char *buffer_end = fslittle_basic_buffer + sizeof(fslittle_basic_buffer);
    std::generate (fslittle_basic_buffer, buffer_end, fslittle_basic_test_random_char);

    bool result = true;
    for (;;) {
        FSLITTLE_DBGLOG("%s:Write test...\n", __func__);
        if (fslittle_basic_test_file_write_littlefs(fslittle_basic_bin_filename_test_10, FSLITTLE_BASIC_KIB_RW) == false) {
            result = false;
            break;
        }

        FSLITTLE_DBGLOG("%s:Read test...\n", __func__);
        if (fslittle_basic_test_file_read_littlefs(fslittle_basic_bin_filename_test_10, FSLITTLE_BASIC_KIB_RW) == false) {
            result = false;
            break;
        }
        break;
    }
    TEST_ASSERT_MESSAGE(result == true, "Expected true result not found");
}

utest::v1::status_t greentea_setup(const size_t number_of_cases)
{
    GREENTEA_SETUP(300, "default_auto");
    return greentea_test_setup_handler(number_of_cases);
}

Case cases[] = {
    /*          1         2         3         4         5         6        7  */
    /* 1234567890123456789012345678901234567890123456789012345678901234567890 */
    Case("FSLITTLE_BASIC_TEST_  : format() test.", FSLITTLE_BASIC_TEST_),
    Case("FSLITTLE_BASIC_TEST_00: fopen()/fgetc()/fprintf()/fclose() test.", FSLITTLE_BASIC_TEST_00),
    Case("FSLITTLE_BASIC_TEST_01: fopen()/fseek()/fclose() test.", FSLITTLE_BASIC_TEST_01),
    /* WARNING: Test case not working but currently not required for PAL support
     * Case("FSLITTLE_BASIC_TEST_02: fopen()/fgets()/fputs()/ftell()/rewind()/remove() test.", FSLITTLE_BASIC_TEST_02) */
    Case("FSLITTLE_BASIC_TEST_03: tmpnam() test.", FSLITTLE_BASIC_TEST_03),
    Case("FSLITTLE_BASIC_TEST_04: fileno() test.", FSLITTLE_BASIC_TEST_04),
    Case("FSLITTLE_BASIC_TEST_05: opendir() basic test.", FSLITTLE_BASIC_TEST_05),
    Case("FSLITTLE_BASIC_TEST_06: fread()/fwrite() file to sdcard.", FSLITTLE_BASIC_TEST_06),
    Case("FSLITTLE_BASIC_TEST_07: sdcard fwrite() file test.", FSLITTLE_BASIC_TEST_07),
    Case("FSLITTLE_BASIC_TEST_08: FATFileSystem::read()/write() test.", FSLITTLE_BASIC_TEST_08),
    Case("FSLITTLE_BASIC_TEST_09: POSIX FILE API fread()/fwrite() test.", FSLITTLE_BASIC_TEST_09),
    Case("FSLITTLE_BASIC_TEST_10: ChanFS read()/write()) test.", FSLITTLE_BASIC_TEST_10),
};


/* Declare your test specification with a custom setup handler */
Specification specification(greentea_setup, cases);

int main()
{
    return !Harness::run(specification);
}