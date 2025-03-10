<template>
    <details-page-title>
        <template v-slot:setting-menu>
            <details-icon-group-wrapper multiIcons>
                <template v-slot:body>
                    <mxs-tooltip-btn
                        :tooltipProps="{ bottom: true }"
                        btnClass="stop-btn"
                        text
                        color="primary"
                        :disabled="serviceState === 'Stopped'"
                        @click="actionHandle('stop')"
                    >
                        <template v-slot:btn-content>
                            <v-icon size="22">$vuetify.icons.mxs_stopped</v-icon>
                        </template>
                        {{ $mxs_t('stop') }} {{ $mxs_tc('services', 1) }}
                    </mxs-tooltip-btn>
                    <mxs-tooltip-btn
                        :tooltipProps="{ bottom: true }"
                        btnClass="start-btn"
                        text
                        color="primary"
                        :disabled="serviceState === 'Started'"
                        @click="actionHandle('start')"
                    >
                        <template v-slot:btn-content>
                            <v-icon size="22">$vuetify.icons.mxs_running</v-icon>
                        </template>
                        {{ $mxs_t('start') }} {{ $mxs_tc('services', 1) }}
                    </mxs-tooltip-btn>
                </template>
            </details-icon-group-wrapper>
            <details-icon-group-wrapper>
                <template v-slot:body>
                    <mxs-tooltip-btn
                        :tooltipProps="{ bottom: true }"
                        btnClass="delete-btn"
                        text
                        color="error"
                        @click="actionHandle('destroy')"
                    >
                        <template v-slot:btn-content>
                            <v-icon size="18">$vuetify.icons.mxs_delete</v-icon>
                        </template>
                        {{ $mxs_t('destroy') }} {{ $mxs_tc('services', 1) }}
                    </mxs-tooltip-btn>
                </template>
            </details-icon-group-wrapper>
        </template>
        <template v-slot:append>
            <portal to="page-header--right">
                <slot name="refresh-rate" />
                <global-search class="ml-4 d-inline-block" />
                <create-resource
                    class="ml-4 d-inline-block"
                    :defFormType="RESOURCE_FORM_TYPES.SERVER"
                    :defRelationshipObj="{
                        id: $route.params.id,
                        type: RELATIONSHIP_TYPES.SERVICES,
                    }"
                />
            </portal>
            <mxs-conf-dlg
                v-model="isConfDlgOpened"
                :title="dialogTitle"
                :saveText="dialogType"
                :type="dialogType"
                :item="currentService"
                :onSave="confirmSave"
            />
            <icon-sprite-sheet
                size="16"
                class="service-state-icon mr-1"
                :frame="$helpers.serviceStateIcon(serviceState)"
            >
                services
            </icon-sprite-sheet>
            <span class="service-state mxs-color-helper text-navigation text-body-2">
                {{ serviceState }}
            </span>
        </template>
    </details-page-title>
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
import goBack from '@share/mixins/goBack'

export default {
    name: 'page-header',
    mixins: [goBack],
    props: {
        onEditSucceeded: { type: Function, required: true },
        currentService: { type: Object, required: true },
    },
    data() {
        return {
            dialogTitle: '',
            dialogType: 'destroy',
            isConfDlgOpened: false,
        }
    },
    computed: {
        ...mapState({
            RESOURCE_FORM_TYPES: state => state.app_config.RESOURCE_FORM_TYPES,
            RELATIONSHIP_TYPES: state => state.app_config.RELATIONSHIP_TYPES,
        }),
        serviceState() {
            return this.currentService.attributes.state
        },
    },
    methods: {
        ...mapActions('service', ['destroyService', 'stopOrStartService']),

        actionHandle(type) {
            this.dialogType = type
            this.dialogTitle = `${this.$mxs_t(type)} ${this.$mxs_tc('services', 1)}`
            this.isConfDlgOpened = true
        },

        async confirmSave() {
            await this.performAsyncLoadingAction(this.dialogType)
        },

        async performAsyncLoadingAction(mode) {
            const { id } = this.currentService
            switch (mode) {
                case 'destroy':
                    await this.destroyService(id)
                    this.goBack()
                    break
                default:
                    await this.stopOrStartService({
                        id,
                        mode,
                        callback: this.onEditSucceeded,
                    })
            }
        },
    },
}
</script>
