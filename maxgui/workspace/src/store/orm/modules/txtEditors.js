/*
 * Copyright (c) 2023 MariaDB plc
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
import QueryTab from '@wsModels/QueryTab'

export default {
    namespaced: true,
    getters: {
        activeRecord: () => QueryTab.getters('activeRecord').txtEditor || {},
        queryTxt: (_, getters) => getters.activeRecord.query_txt || '',
        isVisSidebarShown: (_, getters) => getters.activeRecord.is_vis_sidebar_shown || false,
    },
}
