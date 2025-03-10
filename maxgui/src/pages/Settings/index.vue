<template>
    <page-wrapper>
        <v-sheet class="mt-2">
            <page-header />
            <v-tabs v-model="currentActiveTab" class="v-tabs--mariadb">
                <v-tab v-for="tab in tabs" :key="tab" :href="'#' + tab">
                    {{ tab }}
                </v-tab>

                <v-tabs-items v-model="currentActiveTab">
                    <v-tab-item class="pt-5" :value="tabs[0]">
                        <v-col cols="10">
                            <details-parameters-table
                                v-if="maxscale_parameters"
                                resourceId="maxscale"
                                :parameters="maxscale_parameters"
                                :moduleParameters="moduleParameters"
                                :updateResourceParameters="updateMaxScaleParameters"
                                :onEditSucceeded="fetchMaxScaleParameters"
                                isTree
                                expandAll
                            />
                        </v-col>
                    </v-tab-item>
                </v-tabs-items>
            </v-tabs>
        </v-sheet>
    </page-wrapper>
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
import { mapActions, mapState } from 'vuex'
import PageHeader from './PageHeader'
export default {
    name: 'settings',
    components: {
        PageHeader,
    },
    data() {
        return {
            currentActiveTab: null,
            tabs: [this.$mxs_t('maxScaleParameters')],
            moduleParameters: [],
        }
    },
    computed: {
        ...mapState({
            module_parameters: 'module_parameters',
            maxscale_parameters: state => state.maxscale.maxscale_parameters,
        }),
    },
    watch: {
        currentActiveTab: async function(val) {
            switch (val) {
                case this.$mxs_t('maxScaleParameters'):
                    await Promise.all([
                        this.fetchMaxScaleParameters(),
                        this.fetchModuleParameters('maxscale'),
                    ])
                    this.processModuleParams()
                    break
            }
        },
    },

    methods: {
        ...mapActions({
            fetchModuleParameters: 'fetchModuleParameters',
            fetchMaxScaleParameters: 'maxscale/fetchMaxScaleParameters',
            updateMaxScaleParameters: 'maxscale/updateMaxScaleParameters',
        }),
        processModuleParams() {
            const parameters = this.$helpers.lodash.cloneDeep(this.module_parameters)
            // hard code type for child parameter of log_throttling
            const log_throttingIndex = parameters.findIndex(
                param => param.name === 'log_throttling'
            )
            const log_throttling = parameters[log_throttingIndex]

            const log_throttling_child_params = [
                {
                    name: 'count',
                    type: 'count',
                    modifiable: true,
                    default_value: log_throttling.default_value.count,
                    description: 'Positive integer specifying the number of logged times',
                },
                {
                    name: 'suppress',
                    type: 'duration',
                    modifiable: true,
                    unit: 'ms',
                    default_value: log_throttling.default_value.suppress,
                    description: 'The suppressed duration before the logging of a particular error',
                },
                {
                    name: 'window',
                    type: 'duration',
                    modifiable: true,
                    unit: 'ms',
                    default_value: log_throttling.default_value.window,
                    description: 'The duration that a particular error may be logged',
                },
            ]

            const left = parameters.slice(0, log_throttingIndex + 1)
            const right = parameters.slice(log_throttingIndex + 1)

            this.moduleParameters = [...left, ...log_throttling_child_params, ...right]
        },
    },
}
</script>
