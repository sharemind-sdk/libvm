/*
 * Copyright (C) 2015 Cybernetica
 *
 * Research/Commercial License Usage
 * Licensees holding a valid Research License or Commercial License
 * for the Software may use this file according to the written
 * agreement between you and Cybernetica.
 *
 * GNU General Public License Usage
 * Alternatively, this file may be used under the terms of the GNU
 * General Public License version 3.0 as published by the Free Software
 * Foundation and appearing in the file LICENSE.GPL included in the
 * packaging of this file.  Please review the following information to
 * ensure the GNU General Public License version 3.0 requirements will be
 * met: http://www.gnu.org/copyleft/gpl-3.0.html.
 *
 * For further information, please contact us at sharemind@cyber.ee.
 */

#include "PdpiCache.h"


#include <cassert>
#include <sharemind/Concat.h>
#include <sharemind/MakeUnique.h>


namespace sharemind {

SHAREMIND_DEFINE_EXCEPTION_NOINLINE(sharemind::Exception,
                                    PdpiCache::,
                                    Exception)


PdpiCache::PdpiStartupException::PdpiStartupException(SharemindPd * const pd)
    : m_message(
          std::make_shared<std::string>(
              concat("Failed to start PDPI for PD \"", ::SharemindPd_name(pd),
                     "\"!")))
{}

PdpiCache::PdpiStartupException::PdpiStartupException(
        PdpiStartupException const & copy)
        noexcept(std::is_nothrow_copy_constructible<Exception>::value)
    : Exception(copy)
    , m_message(copy.m_message)
{}

PdpiCache::PdpiStartupException::~PdpiStartupException() noexcept {}

char const * PdpiCache::PdpiStartupException::what() const noexcept
{ return m_message->c_str(); }


PdpiCache::Item::Item(SharemindPd * const pd)
    : m_startException(pd)
{
    assert(pd);

    m_pdpi = ::SharemindPd_newPdpi(pd);
    if (!m_pdpi)
        throw std::bad_alloc();

    m_info.pdpiHandle = nullptr;
    m_info.pdHandle = ::SharemindPd_handle(pd);
    SharemindPdk * pdk = ::SharemindPd_pdk(pd);
    assert(pdk);
    m_info.pdkIndex = ::SharemindPdk_index(pdk);
    SharemindModule * module = ::SharemindPdk_module(pdk);
    assert(module);
    m_info.moduleHandle = ::SharemindModule_handle(module);
}

PdpiCache::Item::~Item() noexcept {
    assert(m_pdpi);
    ::SharemindPdpi_free(m_pdpi);
}

void PdpiCache::Item::start() {
    assert(!m_info.pdpiHandle);
    bool const r = (::SharemindPdpi_start(m_pdpi) == SHAREMIND_MODULE_API_OK);
    if (!r)
        throw m_startException;
    m_info.pdpiHandle = ::SharemindPdpi_handle(m_pdpi);
}

void PdpiCache::Item::stop() noexcept {
    assert(m_info.pdpiHandle);
    ::SharemindPdpi_stop(m_pdpi);
    m_info.pdpiHandle = nullptr;
}


PdpiCache::~PdpiCache() noexcept { destroy(); }

void PdpiCache::reinitialize(std::vector<SharemindPd *> const & pdBindings) {
    auto const newSize = pdBindings.size();
    if (newSize) {
        auto newStorage(makeUnique<ItemStorage[]>(newSize));

        std::size_t i = 0u;
        try {
            for (auto * const pd : pdBindings) {
                new (getItemPtr(newStorage, i)) Item(pd);
                ++i;
            }
        } catch (...) {
            while (i)
                getItemPtr(newStorage, --i)->~Item();
            throw;
        }
        destroy();
        m_storage = std::move(newStorage);
        m_size = newSize;
    } else {
        clear();
    }
}

void PdpiCache::setPdpiFacility(char const * const name,
                                void * const facility,
                                void * const context)
{
    for (std::size_t i = 0u; i < m_size; ++i)
        if (SharemindPdpi_setFacility(getItemPtr(i)->pdpi(),
                                      name,
                                      facility,
                                      context) != SHAREMIND_MODULE_API_OK)
            throw std::bad_alloc();
}

void PdpiCache::clear() noexcept {
    destroy();
    m_storage.reset();
    m_size = 0u;
}

void PdpiCache::startPdpis() {
    std::size_t i = 0u;
    try {
        for (; i < m_size; ++i) {
            getItemPtr(i)->start();
        }
    } catch (...) {
        while (i)
            getItemPtr(--i)->stop();
        throw;
    }
}

void PdpiCache::stopPdpis() noexcept {
    auto i = m_size;
    while (i)
        getItemPtr(--i)->stop();
}

} /* namespace sharemind { */
