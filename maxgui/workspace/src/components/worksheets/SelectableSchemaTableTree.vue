<template>
    <div class="selectable-schema-table-tree">
        <mxs-treeview
            v-model="selectedObjs"
            class="mxs-treeview--objects-select overflow-y-auto mxs-color-helper all-border-separator pa-2 rounded"
            :items="items"
            hoverable
            dense
            open-on-click
            transition
            selectable
            return-object
            :load-children="loadTables"
        >
            <template v-slot:label="{ item: node }">
                <div class="d-flex align-center">
                    <v-icon size="18" color="blue-azure" :class="{ 'ml-1': iconSheet(node) }">
                        {{ iconSheet(node) }}
                    </v-icon>
                    <span class="ml-1 text-truncate d-inline-block node-name">
                        {{ node.name }}
                    </span>
                </div>
            </template>
        </mxs-treeview>
        <div class="input-message-ctr mt-3">
            <p
                v-if="inputMsgObj"
                :class="`v-messages__message ${inputMsgObj.type}--text`"
                data-test="input-msg"
            >
                {{ inputMsgObj.text }}
            </p>
            <p
                v-if="queryErrMsg"
                class="v-messages__message error--text mt-2"
                data-test="query-err-msg"
            >
                {{ queryErrMsg }}
            </p>
        </div>
    </div>
</template>
<script>
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
// ========== Component for selecting schema and table objects ==========
/*
 * Emits
 * $emit('selected-targets', object[])
 */
import { mapState } from 'vuex'
import Worksheet from '@wsModels/Worksheet'
import SchemaSidebar from '@wsModels/SchemaSidebar'
import queries from '@wsSrc/api/queries'
import queryHelper from '@wsSrc/store/queryHelper'
import schemaNodeHelper from '@wsSrc/utils/schemaNodeHelper'

export default {
    name: 'selectable-schema-table-tree',
    props: {
        connId: { type: String, required: true },
        preselectedSchemas: { type: Array, default: () => [] },
        triggerDataFetch: { type: Boolean, required: true },
        excludeNonFkSupportedTbl: { type: Boolean, default: false },
    },
    data() {
        return {
            selectedObjs: [],
            items: [],
            inputMsgObj: null,
            queryErrMsg: '',
        }
    },
    computed: {
        ...mapState({
            NODE_GROUP_TYPES: state => state.mxsWorkspace.config.NODE_GROUP_TYPES,
            NODE_TYPES: state => state.mxsWorkspace.config.NODE_TYPES,
            FK_SUPPORTED_ENGINE: state => state.mxsWorkspace.config.FK_SUPPORTED_ENGINE,
        }),
        activeRequestConfig() {
            return Worksheet.getters('activeRequestConfig')
        },
        categorizeObjs() {
            return this.selectedObjs.reduce(
                (obj, o) => {
                    // SCHEMA nodes will be included in selectedObjs even though those have no tables
                    if (o.type === this.NODE_TYPES.SCHEMA) obj.emptySchemas.push(o.name)
                    else
                        obj.targets.push({
                            tbl: o.name,
                            schema: o.parentNameData[this.NODE_TYPES.SCHEMA],
                        })
                    return obj
                },
                { targets: [], emptySchemas: [] }
            )
        },
        targets() {
            return this.categorizeObjs.targets
        },
        emptySchemas() {
            return this.categorizeObjs.emptySchemas
        },
    },
    watch: {
        triggerDataFetch: {
            immediate: true,
            async handler(v) {
                if (v) {
                    this.items = []
                    await this.fetchSchemas()
                    await this.handlePreselectedSchemas()
                }
            },
        },
        selectedObjs: {
            deep: true,
            handler(v) {
                if (v.length) {
                    if (!this.targets.length)
                        this.inputMsgObj = {
                            type: 'error',
                            text: this.$mxs_t('errors.emptyVisualizeSchema'),
                        }
                    else if (this.emptySchemas.length)
                        this.inputMsgObj = {
                            type: 'warning',
                            text: this.$mxs_t('warnings.ignoredVisualizeSchemas'),
                        }
                    else this.inputMsgObj = null
                } else this.inputMsgObj = null
            },
        },
        targets: {
            deep: true,
            immediate: true,
            handler(v) {
                this.$emit('selected-targets', v)
            },
        },
    },
    methods: {
        iconSheet(node) {
            if (node.type === this.NODE_TYPES.SCHEMA) return '$vuetify.icons.mxs_database'
        },
        async fetchSchemas() {
            const [e, res] = await this.$helpers.to(
                queries.post({
                    id: this.connId,
                    body: { sql: SchemaSidebar.getters('schemaSql') },
                    config: this.activeRequestConfig,
                })
            )

            if (e) this.queryErrMsg = this.$mxs_t('errors.retrieveSchemaObj')
            else {
                const result = this.$typy(res, 'data.data.attributes.results[0]').safeObject
                if (this.$typy(result, 'errno').isDefined)
                    this.queryErrMsg = this.$helpers.queryResErrToStr(result)
                else {
                    this.queryErrMsg = ''
                    const { nodes } = schemaNodeHelper.genNodeData({
                        queryResult: result,
                        nodeAttrs: { isEmptyChildren: true },
                    })
                    this.items = nodes
                }
            }
        },
        async handlePreselectedSchemas() {
            const nodes = this.items.filter(n => this.preselectedSchemas.includes(n.qualified_name))
            let selectedObjs = []
            for (const node of nodes) {
                await this.loadTables(node)
                /**
                 * If preselected schemas have no tables, add the schema node to selectedObjs
                 * so the input validation message can be shown.
                 */
                if (node.children.length) selectedObjs = [...selectedObjs, ...node.children]
                else selectedObjs = [...selectedObjs, node]
            }
            this.selectedObjs = selectedObjs
        },
        async loadTables(node) {
            const { nodes } = await queryHelper.getChildNodeData({
                connId: this.connId,
                nodeGroup: schemaNodeHelper.genNodeGroup({
                    parentNode: node,
                    type: this.NODE_GROUP_TYPES.TBL_G,
                }),
                nodeAttrs: {
                    isLeaf: true,
                },
                config: this.activeRequestConfig,
            })
            if (this.excludeNonFkSupportedTbl)
                node.children = nodes.filter(n => n.data.ENGINE === this.FK_SUPPORTED_ENGINE)
            else node.children = nodes
        },
    },
}
</script>

<style lang="scss" scoped>
.selectable-schema-table-tree {
    .input-message-ctr {
        min-height: 24px;
    }
    .mxs-treeview--objects-select {
        max-height: 500px;
        font-size: 0.875rem;
        background-color: #fbfbfb;
    }
}
</style>
