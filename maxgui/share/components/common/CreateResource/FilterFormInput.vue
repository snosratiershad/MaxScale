<template>
    <div class="mb-2">
        <module-parameters ref="moduleInputs" moduleName="module" :modules="resourceModules" />
    </div>
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
import ModuleParameters from './ModuleParameters'

export default {
    name: 'filter-form-input',
    components: {
        ModuleParameters,
    },
    props: {
        resourceModules: { type: Array, required: true },
    },

    methods: {
        getValues() {
            const { moduleId, parameters } = this.$refs.moduleInputs.getModuleInputValues()
            return {
                moduleId: moduleId,
                parameters: parameters,
            }
        },
    },
}
</script>
