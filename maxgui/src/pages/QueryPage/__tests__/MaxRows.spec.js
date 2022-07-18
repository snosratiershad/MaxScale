/*
 * Copyright (c) 2020 MariaDB Corporation Ab
 *
 * Use of this software is governed by the Business Source License included
 * in the LICENSE.TXT file and at www.mariadb.com/bsl11.
 *
 * Change Date: 2026-07-11
 *
 * On the date above, in accordance with the Business Source License, use
 * of this software will be governed by version 2 or later of the General
 *  Public License.
 */

import mount from '@tests/unit/setup'
import MaxRows from '@/pages/QueryPage/MaxRows'
import { getErrMsgEle } from '@tests/unit/utils'

const mountFactory = opts =>
    mount({
        shallow: false,
        component: MaxRows,
        ...opts,
    })

describe(`MaxRows - created hook and data communication tests`, () => {
    let wrapper
    it(`Should pass accurate data to v-combobox via props`, () => {
        const SQL_DEF_MAX_ROWS_OPTS = [10, 50, 100]
        const mockvalue = 1000
        wrapper = mountFactory({
            shallow: true,
            propsData: { value: mockvalue },
            attrs: { items: SQL_DEF_MAX_ROWS_OPTS },
        })
        const dropdown = wrapper.findComponent({ name: 'v-combobox' })
        const { value, items } = dropdown.vm.$props
        expect(value).to.be.equals(mockvalue)
        expect(items).to.be.deep.equals(SQL_DEF_MAX_ROWS_OPTS)
    })
    describe(`Input validation tests`, () => {
        let wrapper, dropdown
        beforeEach(() => {
            wrapper = mountFactory({ propsData: { hideDetails: 'auto' } })
            dropdown = wrapper.findComponent({ name: 'v-combobox' })
        })
        it(`Should show required error message if input is empty`, async () => {
            await wrapper.vm.onInput({ srcElement: { value: '' } })
            wrapper.vm.$nextTick(() => {
                expect(getErrMsgEle(dropdown).text()).to.be.equals(
                    wrapper.vm.$t('errors.requiredInput', { inputName: wrapper.vm.$t('maxRows') })
                )
            })
        })
        it(`Should show non integer error message if input is a string`, async () => {
            await wrapper.vm.onInput({ srcElement: { value: 'abc' } })
            wrapper.vm.$nextTick(() => {
                expect(getErrMsgEle(dropdown).text()).to.be.equals(
                    wrapper.vm.$t('errors.nonInteger')
                )
            })
        })
        it(`Should show 'largerThanZero' if input is zero`, async () => {
            await wrapper.vm.onInput({ srcElement: { value: 0 } })
            wrapper.vm.$nextTick(() => {
                expect(getErrMsgEle(dropdown).text()).to.be.equals(
                    wrapper.vm.$t('errors.largerThanZero')
                )
            })
        })
    })
})
