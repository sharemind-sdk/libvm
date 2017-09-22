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


#include <cstddef>
#include <memory>
#include <sharemind/Exception.h>
#include <sharemind/ExceptionMacros.h>
#include <sharemind/libmodapi/libmodapi.h>
#include <sharemind/module-apis/api_0x1.h>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>


namespace sharemind {

class PdpiCache final {

public: /* Types: */

    SHAREMIND_DECLARE_EXCEPTION_NOINLINE(sharemind::Exception, Exception);

    class PdpiStartupException final: public Exception {

    public: /* Methods: */

        PdpiStartupException(SharemindPd * const pd);
        PdpiStartupException(PdpiStartupException const &)
                noexcept(std::is_nothrow_copy_constructible<Exception>::value);
        ~PdpiStartupException() noexcept final override;

        char const * what() const noexcept final override;

    private: /* Methods: */

        std::shared_ptr<std::string const> m_message;

    };

private: /* Types: */

    class Item final {

    public: /* Methods: */

        Item(SharemindPd * const pd);
        ~Item() noexcept;

        SharemindPdpi * pdpi() const noexcept { return m_pdpi; }

        SharemindModuleApi0x1PdpiInfo const & info() const noexcept
        { return m_info; }

        void start();
        void stop() noexcept;

    private: /* Fields: */

        PdpiStartupException m_startException;
        SharemindPdpi * m_pdpi;
        SharemindModuleApi0x1PdpiInfo m_info;

    };

    using ItemStorage =
            typename std::aligned_storage<sizeof(Item), alignof(Item)>::type;
    using Storage = std::unique_ptr<ItemStorage[]>;

public: /* Methods: */

    ~PdpiCache() noexcept;

    SharemindPdpi * pdpi(std::size_t const index) const noexcept
    { return (index < m_size) ? getItemPtr(index)->pdpi() : nullptr; }

    void setPdpiFacility(char const * const name,
                         void * const facility,
                         void * const context);

    SharemindModuleApi0x1PdpiInfo const * info(std::size_t const index)
            const noexcept
    { return (index < m_size) ? &getItemPtr(index)->info() : nullptr; }

    std::size_t size() const noexcept { return m_size; }

    void reinitialize(std::vector<SharemindPd *> const & pdBindings);
    void clear() noexcept;
    void startPdpis();
    void stopPdpis() noexcept;

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
