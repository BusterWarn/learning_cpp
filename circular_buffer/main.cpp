#include <array>
#include <cassert>
#include <chrono>
#include <deque>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string_view>
#include <type_traits>

template<typename Pointer>
concept is_pointer_like = std::is_pointer_v<Pointer> || requires(Pointer pointer) { pointer.operator->(); };

template<typename Payload>
concept event_payload = 
(is_pointer_like<Payload> && requires(Payload payload, std::ostream& os)
{
  {payload->to_osstream(os)} -> std::same_as<void>;
})
||
(!is_pointer_like<Payload> && requires(Payload payload, std::ostream& os)
{
  {payload.to_osstream(os)} -> std::same_as<void>;
});

struct signal
{
  enum class transmission_type : std::uint8_t { SENT, RECEIVED };
  enum class signal_type : std::uint8_t { SIGNAL1, SIGNAL2, SIGNAL3, SIGNAL4, SIGNAL5 };

  [[nodiscard]] inline static constexpr auto
  enum_to_cstr(const transmission_type& status) noexcept -> const char*
  {
    switch(status) {
      case transmission_type::SENT: return "SENT";
      case transmission_type::RECEIVED: return "RECD";
    }
    return "Unknown";
  }

  [[nodiscard]] static constexpr auto
  enum_to_cstr(const signal_type& s) noexcept -> const char*
  {
    switch (s)
    {
      case signal_type::SIGNAL1: return "Signal 1";
      case signal_type::SIGNAL2: return "Signal 2";
      case signal_type::SIGNAL3: return "Signal 3";
      case signal_type::SIGNAL4: return "Signal 4";
      case signal_type::SIGNAL5: return "Signal 5";
    }
    return "Unknown Signal";
  }

  inline friend std::ostream& operator<<(std::ostream& os, const signal& sig) noexcept
  {
    sig.to_osstream(os);
    return os;
  }

  inline void
  to_osstream(std::ostream& os) const noexcept
  {
    os << ' ' << enum_to_cstr(_status)
       << ' ' << enum_to_cstr(_signal)
       << _debug_string
       << '.';
  }

  transmission_type _status;
  signal_type _signal;
  std::string _debug_string;
};

template<event_payload Event, std::size_t N>
class circular_buffer_array
{
  static_assert((N & (N - 1)) == 0 && N != 0, "Circular Buffer size N must be a power of 2");
public:
  // Logs an event to the buffer, utilizing C++20's Abbreviated Function Templates for concise syntax.
  // `event_payload auto&& message` allows for perfect forwarding, keeping the value category (lvalue/rvalue) of `message`.
  // `std::forward<decltype(message)>(message)` is used to forward the `message` argument to the buffer without any extra copying or moving.
  inline auto
  log(event_payload auto&& message) noexcept
  {
    buffer[write_index] = {std::chrono::system_clock::now(), std::forward<decltype(message)>(message)};
    write_index = (write_index + 1) & (N - 1);  // Only works if N is a power of 2
    active_entries = __builtin_expect(active_entries + 1 < N, 1) ? active_entries + 1 : N;
  }

  inline auto
  to_osstream(std::ostream& os) const noexcept
  {
    for(std::size_t i = 0; i < active_entries; ++i)
    {
      const auto index = (write_index + N - active_entries + i) % N;
      const auto& [timestamp, message] = buffer[index];
      const auto time = std::chrono::system_clock::to_time_t(timestamp);
      os << std::put_time(std::localtime(&time), "%Y-%m-%d %X") << ' ' << message << '\n';
    }
  }

  [[nodiscard]] inline auto
  to_string() const noexcept -> std::string
  {
    std::ostringstream oss;
    to_osstream(oss);
    return oss.str();
  }

private:
  std::array<std::pair<std::chrono::system_clock::time_point, Event>, N> buffer = {};
  std::size_t write_index = 0;
  std::size_t active_entries = 0;
};

template<event_payload Event, std::size_t N>
class circular_buffer_deque {
public:
  // Lvalue reference
  inline auto
  log(const Event& message)
  {
    if (buffer.size() == N)
    {
        buffer.pop_front();  // Remove the oldest element
    }
    buffer.push_back({std::chrono::system_clock::now(), message});
  }

  // Rvalue reference
  inline auto
  log(Event&& message)
  {
    if (buffer.size() == N)
    {
        buffer.pop_front();  // Remove the oldest element
    }
    buffer.push_back({std::chrono::system_clock::now(), std::move(message)});
  }

  inline auto
  to_osstream(std::ostream& os) const
  {
    for (const auto& [timestamp, message] : buffer)
    {
      const auto time = std::chrono::system_clock::to_time_t(timestamp);
      os << std::put_time(std::localtime(&time), "%Y-%m-%d %X") << ' ' << message << '\n';
    }
  }

  [[nodiscard]] inline auto
  to_string() const -> std::string
  {
    std::ostringstream oss;
    to_osstream(oss);
    return oss.str();
  }

private:
  std::size_t max_size;
  std::deque<std::pair<std::chrono::system_clock::time_point, Event>> buffer;
};

int main()
{
  const std::array<signal, 5> events_to_log =
  {
    signal
    {
      ._status = signal::transmission_type::SENT,
      ._signal = signal::signal_type::SIGNAL1,
      ._debug_string = "This will be removed..."
    },
    signal
    {
      ._status = signal::transmission_type::SENT,
      ._signal = signal::signal_type::SIGNAL2
    },
    signal
    {
      ._status = signal::transmission_type::RECEIVED,
      ._signal = signal::signal_type::SIGNAL3
    },
    signal
    {
      ._status = signal::transmission_type::SENT,
      ._signal = signal::signal_type::SIGNAL4
    },
    signal
    {
      ._status = signal::transmission_type::RECEIVED,
      ._signal = signal::signal_type::SIGNAL5,
      ._debug_string = " This is a weird signal..."
    }
  };

  circular_buffer_array<signal, 4> array_logger;
  circular_buffer_deque<signal, 4> deque_logger;
  for (const auto& event_to_log : events_to_log)
  {
    array_logger.log(event_to_log);
    deque_logger.log(event_to_log);
  }

  assert(array_logger.to_string() == deque_logger.to_string());
  array_logger.to_osstream(std::cout);

  // Just to show that concept accepts pointers as well.
  circular_buffer_array<signal*, 4> array_logger_with_pointers;
  auto *const signal_p = new signal; // This can be const
  array_logger_with_pointers.log(std::move(signal_p)); // Must use std::move here...
  delete signal_p;

  circular_buffer_deque<std::unique_ptr<signal>, 4> deque_logger_with_smart_pointers;
  deque_logger_with_smart_pointers.log(std::make_unique<signal>());
  auto signal_up = std::make_unique<signal>(); // This cannot be const
  deque_logger_with_smart_pointers.log(std::move(signal_up));
}
