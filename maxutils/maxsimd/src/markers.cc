/*
 * Copyright (c) 2021 MariaDB Corporation Ab
 * Copyright (c) 2023 MariaDB plc, Finnish Branch
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2028-01-30
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */

#include "markers.hh"

namespace
{
struct  ThisThread
{
    maxsimd::Markers markers;
};

thread_local ThisThread this_thread;
}

namespace maxsimd
{
Markers* markers()
{
    this_thread.markers.clear();
    return &this_thread.markers;
}
}
