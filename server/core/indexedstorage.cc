/*
 * Copyright (c) 2016 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2026-10-04
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 * Public License.
 */

#include <maxscale/indexedstorage.hh>

namespace maxscale
{

size_t IndexedStorage::clear()
{
    size_t rv = 0;

    for (uint64_t key = 0; key < m_local_data.size(); ++key)
    {
        auto* pData = m_local_data[key];
        auto deleter = m_data_deleters[key];
        auto sizer = m_data_sizers[key];

        if (pData && sizer)
        {
            rv += sizer(pData);
        }

        if (pData && deleter)
        {
            deleter(pData);
        }
    }

    m_local_data.clear();
    m_data_deleters.clear();
    m_data_sizers.clear();

    return rv;
}

IndexedStorage::~IndexedStorage()
{
    clear();
}
}
