<template>
    <mxs-collapse
        :toggleOnClick="() => (showTable = !showTable)"
        :isContentVisible="showTable"
        :title="$mxs_t('routingTargets')"
        :titleInfo="tableRows.length"
        :onAddClick="isAdmin ? onEdit : null"
        :addBtnText="isAdmin ? $mxs_t('edit') : ''"
    >
        <data-table
            :search="search_keyword"
            :headers="tableHeader"
            :data="tableRows"
            :noDataText="$mxs_t('noEntity', { entityName: $mxs_t('routingTargets') })"
            sortBy=""
            :loading="isLoading"
            showActionsOnHover
            showAll
        >
            <template v-slot:id="{ data: { item: { id, state, type } } }">
                <icon-sprite-sheet
                    size="13"
                    class="mr-1 state-icon"
                    :frame="getStatusIcon({ state, type })"
                >
                    {{ type }}
                </icon-sprite-sheet>
                <router-link
                    :key="id"
                    v-mxs-highlighter="{ keyword: search_keyword, txt: id }"
                    :to="`/dashboard/${type}/${id}`"
                    class="rsrc-link"
                >
                    {{ id }}
                </router-link>
            </template>
            <template v-if="isAdmin" v-slot:actions="{ data: { item } }">
                <v-btn icon @click="onDelete(item)">
                    <v-icon size="20" color="error">
                        $vuetify.icons.mxs_unlink
                    </v-icon>
                </v-btn>
            </template>
        </data-table>
        <mxs-conf-dlg
            v-model="isConfDlgOpened"
            :title="dialogTitle"
            saveText="unlink"
            type="unlink"
            :item="$typy(targetItems, '[0]').safeObjectOrEmpty"
            :onSave="confirmDelete"
        />
        <routing-target-dlg
            v-model="isRoutingTargetDlgOpened"
            :title="dialogTitle"
            :routerId="routerId"
            :onSave="confirmEdit"
            :initialRoutingTargetHash="initialRoutingTargetHash"
            @selected-items="targetItems = $event"
        />
    </mxs-collapse>
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

import { OVERLAY_TRANSPARENT_LOADING } from '@share/overlayTypes'
import { mapState, mapGetters } from 'vuex'
import RoutingTargetDlg from './RoutingTargetDlg.vue'
import asyncEmit from '@share/mixins/asyncEmit'

export default {
    name: 'routing-target-table',
    components: { RoutingTargetDlg },
    mixins: [asyncEmit],
    props: {
        tableRows: { type: Array, required: true },
        routerId: { type: String, required: true }, // the id of the MaxScale object being altered
    },
    data() {
        return {
            isMounting: true,
            showTable: true,
            //---------------- common
            dialogTitle: '',
            targetItems: [],
            isConfDlgOpened: false,
            isRoutingTargetDlgOpened: false,
        }
    },
    computed: {
        ...mapState({
            overlay_type: state => state.mxsApp.overlay_type,
            search_keyword: 'search_keyword',
        }),
        ...mapGetters({ isAdmin: 'user/isAdmin' }),
        isLoading() {
            return this.isMounting ? true : this.overlay_type === OVERLAY_TRANSPARENT_LOADING
        },
        tableHeader() {
            return [
                {
                    text: 'Id',
                    value: 'id',
                    autoTruncate: true,
                    sortable: false,
                    width: '75%',
                },
                {
                    text: 'Type',
                    value: 'type',
                    sortable: false,
                    width: '25%',
                },
                { text: '', value: 'action', sortable: false },
            ]
        },
        initialRoutingTargets() {
            return this.formatRelationshipData(this.tableRows)
        },
        initialRoutingTargetHash() {
            return this.$helpers.hashMapByPath({
                arr: this.initialRoutingTargets,
                path: 'type',
            })
        },
        targetItemsHash() {
            return this.$helpers.hashMapByPath({ arr: this.targetItems, path: 'type' })
        },
        newRoutingTargetHash() {
            const diff = this.$helpers.arrOfObjsDiff({
                base: this.initialRoutingTargets,
                newArr: this.targetItems,
                idField: 'id',
            })
            const removedObjs = diff.get('removed')
            const addedObjs = diff.get('added')

            let newHash = this.$helpers.lodash.cloneDeep(this.initialRoutingTargetHash)
            addedObjs.forEach(obj => {
                const type = obj.type
                if (newHash[type]) newHash[type].push(obj)
                else newHash[type] = [obj]
            })
            removedObjs.forEach(obj => {
                const type = obj.type
                if (newHash[type]) newHash[type] = newHash[type].filter(item => item.id !== obj.id)
            })

            return newHash
        },
    },
    async mounted() {
        await this.$helpers.delay(400).then(() => (this.isMounting = false))
    },
    methods: {
        /**
         * @param {Array} arr - array of object, each object must have id and type attributes
         * @returns {Array} - returns valid relationship array data
         */
        formatRelationshipData(arr) {
            return arr.map(item => ({ id: item.id, type: item.type }))
        },
        getStatusIcon({ state, type }) {
            switch (type) {
                case 'services':
                    return this.$helpers.serviceStateIcon(state)
                case 'servers':
                    return this.$helpers.serverStateIcon(state)
                case 'monitors':
                    return this.$helpers.monitorStateIcon(state)
            }
        },
        async emitUpdateEvt({ type, data }) {
            await this.asyncEmit('on-relationship-update', {
                type,
                data,
                isUpdatingRouteTarget: true,
            })
        },
        onDelete(item) {
            this.targetItems = this.formatRelationshipData([item])
            this.dialogTitle = `${this.$mxs_t('unlink')} ${this.$mxs_tc(item.type, 1)}`
            this.isConfDlgOpened = true
        },
        async confirmDelete() {
            const initialHash = this.initialRoutingTargetHash
            const hash = this.targetItemsHash
            for (const type of Object.keys(hash))
                await this.emitUpdateEvt({
                    type,
                    data: this.$helpers.lodash.xorWith(
                        initialHash[type],
                        hash[type],
                        (a, b) => a.id === b.id
                    ),
                })
        },
        onEdit() {
            this.dialogTitle = `${this.$mxs_t(`editEntity`, {
                entityName: this.$mxs_t('routingTargets'),
            })}`
            this.isRoutingTargetDlgOpened = true
        },
        async confirmEdit() {
            for (const type of Object.keys(this.newRoutingTargetHash)) {
                const newData = this.newRoutingTargetHash[type]
                if (!this.$helpers.lodash.isEqual(this.initialRoutingTargetHash[type], newData))
                    await this.emitUpdateEvt({ type, data: newData })
            }
        },
    },
}
</script>
