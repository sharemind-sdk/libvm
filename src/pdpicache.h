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

#ifndef SHAREMIND_LIBVM_PDPICACHE_H
#define SHAREMIND_LIBVM_PDPICACHE_H

#ifndef SHAREMIND_INTERNAL_
#error including an internal header!
#endif


#include <cassert>
#include <cstddef>
#include <sharemind/Concat.h>
#include <sharemind/Exception.h>
#include <sharemind/libmodapi/libmodapi.h>
#include <sharemind/MakeUnique.h>
#include <sharemind/module-apis/api_0x1.h>
#include <type_traits>
#include <utility>
#include <vector>


namespace sharemind {

class PdpiCache {

public: /* Types: */

    SHAREMIND_DEFINE_EXCEPTION(std::exception, Exception);
    SHAREMIND_DEFINE_EXCEPTION_CONST_STDSTRING(Exception,
                                               PdpiStartupExceptionBase);
    class PdpiStartupException: public PdpiStartupExceptionBase {

    public: /* Methods: */

        PdpiStartupException(SharemindPd * const pd)
            : PdpiStartupExceptionBase(concat("Failed to start PDPI for PD \"",
                                              ::SharemindPd_name(pd), "\"!"))
        {}

    };

private: /* Types: */

    class Item {

    public: /* Methods: */

        Item(SharemindPd * const pd)
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

        ~Item() noexcept {
            assert(m_pdpi);
            ::SharemindPdpi_free(m_pdpi);
        }

        SharemindPdpi * pdpi() const noexcept { return m_pdpi; }

        SharemindModuleApi0x1PdpiInfo const & info() const noexcept
        { return m_info; }

        void start() {
            assert(!m_info.pdpiHandle);
            bool const r =
                    (::SharemindPdpi_start(m_pdpi) == SHAREMIND_MODULE_API_OK);
            if (!r)
                throw m_startException;
            m_info.pdpiHandle = ::SharemindPdpi_handle(m_pdpi);
        }

        void stop() noexcept {
            assert(m_info.pdpiHandle);
            ::SharemindPdpi_stop(m_pdpi);
            m_info.pdpiHandle = nullptr;
        }

    private: /* Fields: */

        PdpiStartupException m_startException;
        SharemindPdpi * m_pdpi;
        SharemindModuleApi0x1PdpiInfo m_info;

    };

    using ItemStorage =
            typename std::aligned_storage<sizeof(Item), alignof(Item)>::type;
    using Storage = std::unique_ptr<ItemStorage[]>;

public: /* Methods: */

    ~PdpiCache() noexcept { destroy(); }

    SharemindPdpi * pdpi(std::size_t const index) const noexcept
    { return (index < m_size) ? getItemPtr(index)->pdpi() : nullptr; }

    SharemindModuleApi0x1PdpiInfo const * info(std::size_t const index)
            const noexcept
    { return (index < m_size) ? &getItemPtr(index)->info() : nullptr; }

    std::size_t size() const noexcept { return m_size; }

    void reinitialize(std::vector<SharemindPd *> const & pdBindings) {
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

    void clear() noexcept {
        destroy();
        m_storage.reset();
        m_size = 0u;
    }

    void startPdpis() {
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

    void stopPdpis() noexcept {
        auto i = m_size;
        while (i)
            getItemPtr(--i)->stop();
    }

private: /* Methods: */

    Item * getItemPtr(std::size_t const index) const noexcept
    { return getItemPtr(m_storage, index); }

    static Item * getItemPtr(Storage const & storage, std::size_t const index)
            noexcept
    { return reinterpret_cast<Item *>(std::addressof(storage[index])); }

    void destroy() noexcept {
        auto i = m_size;
        while (i)
            getItemPtr(--i)->~Item();
    }

private: /* Fields: */

    std::size_t m_size = 0u;
    Storage m_storage;

};

} /* namespace sharemind { */

#endif /* SHAREMIND_LIBVM_PDPICACHE_H */
