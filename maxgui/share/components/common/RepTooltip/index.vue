<template>
    <v-menu
        offset-y
        transition="slide-y-transition"
        :close-on-content-click="false"
        open-on-hover
        content-class="shadow-drop mxs-color-helper text-navigation"
        allow-overflow
        :max-height="450"
        v-bind="{ ...$attrs }"
        v-on="$listeners"
    >
        <template v-slot:activator="{ on }">
            <slot name="activator" :on="on" />
        </template>
        <v-sheet class="py-4 px-3 text-body-2">
            <div class="px-1 py-1 font-weight-bold ">
                {{ isMaster ? $mxs_t('slaveRepStatus') : $mxs_t('replicationStatus') }}
            </div>
            <v-divider class="mxs-color-helper border-separator" />

            <template v-if="isMaster">
                <table class="rep-table px-1">
                    <tr v-for="(slaveStat, i) in getSlaveStatus" :key="`${i}`" class="mb-1">
                        <td>
                            <icon-sprite-sheet
                                size="13"
                                class="mr-1 rep-icon"
                                :frame="$helpers.repStateIcon(slaveStat.overall_replication_state)"
                            >
                                replication
                            </icon-sprite-sheet>
                        </td>
                        <td>
                            <div class="d-flex align-center fill-height">
                                <mxs-truncate-str
                                    :tooltipItem="{ txt: `${slaveStat.id}`, nudgeTop: 10 }"
                                    :maxWidth="300"
                                />
                                <span class="ml-1 mxs-color-helper text-grayed-out">
                                    (+{{ slaveStat.overall_seconds_behind_master }}s)
                                </span>
                            </div>
                        </td>
                    </tr>
                </table>
            </template>
            <!-- Slave server replication status, serverInfo length is always <= 1 -->
            <table v-else class="rep-table px-1">
                <tbody
                    v-for="(stat, i) in $helpers.getRepStats(serverInfo[0])"
                    :key="`${i}`"
                    :class="{ 'tbody-src-replication': !isMaster }"
                >
                    <tr v-for="(value, key) in stat" :key="`${key}`">
                        <td class="pr-5">
                            {{ key }}
                        </td>
                        <td>
                            <div class="d-flex align-center fill-height">
                                <icon-sprite-sheet
                                    v-if="key === 'replication_state'"
                                    size="13"
                                    class="mr-1 rep-icon"
                                    :frame="$helpers.repStateIcon(value)"
                                >
                                    replication
                                </icon-sprite-sheet>
                                <mxs-truncate-str
                                    :tooltipItem="{ txt: `${value}`, nudgeTop: 10 }"
                                    :maxWidth="400"
                                />
                            </div>
                        </td>
                    </tr>
                </tbody>
            </table>
        </v-sheet>
    </v-menu>
</template>

<script>
/*
 * Copyright (c) 2020 MariaDB Corporation Ab
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
export default {
    name: 'rep-tooltip',
    inheritAttrs: false,
    props: {
        serverInfo: { type: Array, required: true },
        isMaster: { type: Boolean, required: false },
    },
    computed: {
        /**
         * If isMaster is true, the component is used to get overall slave replication status
         */
        getSlaveStatus() {
            if (!this.serverInfo.length) return []
            const slaveStats = []
            this.serverInfo.forEach(item => {
                const repStats = this.$helpers.getRepStats(item)
                slaveStats.push({
                    id: item.name,
                    overall_replication_state: this.$helpers.getMostFreq({
                        arr: repStats,
                        pickBy: 'replication_state',
                    }),
                    overall_seconds_behind_master: this.$helpers.getMin({
                        arr: repStats,
                        pickBy: 'seconds_behind_master',
                    }),
                })
            })
            return slaveStats
        },
    },
}
</script>

<style lang="scss" scoped>
.tbody-src-replication {
    &:not(:last-of-type) {
        &::after,
        &:first-of-type::before {
            content: '';
            display: block;
            height: 12px;
        }
    }
}
.rep-table {
    td {
        white-space: nowrap;
        height: 24px;
        line-height: 1.5;
    }
}
</style>
