/* 
 * The following userspace module uses Kernel user-space
 * interrupts in order to have a working infrastructure
 * for quadrature encoders.
 *
 * References:
 * https://github.com/tweej/HighLatencyGPIO
 *
 */

#include <iostream>
#include <stdexcept>
#include <cmath>
#include "QuadratureEncoder.h"


QuadratureEncoder::QuadratureEncoder(const int &pin_a, const int &pin_b)
{
    std::cout << __PRETTY_FUNCTION__ << std::endl;
    
    /* Register our local GPIO callbacks to use for SW interrupts */
    _channel_a_callback = std::bind(&QuadratureEncoder::ISR_ChannelA, this);
    _channel_b_callback = std::bind(&QuadratureEncoder::ISR_ChannelB, this);
    
    /* Initialize channels GPIO objects and assign local callbacks */
    _gpio_a = new GPIO(pin_a, GPIO::Edge::BOTH, _channel_a_callback);
    _gpio_b = new GPIO(pin_b, GPIO::Edge::BOTH, _channel_b_callback);
    
    /* Reset statistics */
    _channel_a_isr_cnt = 0;
    _channel_b_isr_cnt = 0;

    /* Useful information to be printed regarding set-up */
    std::cout << "INFO: Userspace quadrature encoder created @ (pinA="
              << pin_a
              << " pinB="
              << pin_b
              << ")" << std::endl;
}


QuadratureEncoder::~QuadratureEncoder(void)
{
    delete _gpio_a;
    delete _gpio_b;
}


/* External API for the user, exposed to be used by higher classes */
int QuadratureEncoder::GetPosition(void)
{
    std::cout << "INFO: Current position counter " 
              << std::abs(_counter) << std::endl;
    return _counter;
}


std::chrono::nanoseconds QuadratureEncoder::GetPeriod(void)
{
    std::cout << "INFO: Last pulsewidth duration " << _pulse_period_ns.count() 
              << " ns" << std::endl;

    return _pulse_period_ns;
}


void QuadratureEncoder::SetZero(void)
{
    _counter = 0;
}


QuadratureEncoder::Direction QuadratureEncoder::GetDirection(void)
{
    if(_direction == Direction::CW) {
        std::cout << "INFO: Current direction        +" << std::endl;
    } else
        std::cout << "INFO: Current direction        -" << std::endl;
    return _direction;
}


void QuadratureEncoder::PrintStats(void)
{
    std::cout << "INFO: ChannelA interrupts      " << _channel_a_isr_cnt << std::endl;
    std::cout << "INFO: ChannelB interrupts      " << _channel_b_isr_cnt << std::endl;
    (void) GetDirection();
    (void) GetPeriod();
}


/* Internal Quadrature Encoder ISR Handlers */
void QuadratureEncoder::ISR_ChannelA(void)
{
    char delta;
    char a, b;
    char current_packed_read_a;

    /* Obtain the pulsewidth between transitions */
    TrackGPIOPulseWidth();

    /* Convert enum class to actual zero or one */
    _gpio_a->getValue() == GPIO::Value::HIGH ? a = 1 : a = 0;
    _gpio_b->getValue() == GPIO::Value::HIGH ? b = 1 : b = 0;

    /* Convert binary input to decimal value */
    current_packed_read_a = (a << 1) | (b << 0);
    /* Increment, or decrement depending on matrix */
    delta = _qem[_prev_packed_read_a * 4 + current_packed_read_a];
    
    /* Update our rotation direction now */
    if(delta == -1)            _direction = Direction::CCW;
    else if(delta == 1)        _direction = Direction::CW;
    
    /* Update our local tracking variable */
    _counter += delta;
    
    /* Update our previous reading */
    _prev_packed_read_a = current_packed_read_a;

    /* Debug statistics */
    _channel_a_isr_cnt++;
}


void QuadratureEncoder::ISR_ChannelB(void)
{
    char delta;
    char a, b;
    char current_packed_read_b;

    /* Convert enum class to actual zero or one */
    _gpio_a->getValue() == GPIO::Value::HIGH ? a = 1 : a = 0;
    _gpio_b->getValue() == GPIO::Value::HIGH ? b = 1 : b = 0;
    
    /* Obtain the pulsewidth between transitions */
    TrackGPIOPulseWidth();

    /* Convert binary input to decimal value */
    current_packed_read_b = (a << 1) | (b << 0);
    /* Increment, or decrement depending on matrix */
    delta = _qem[_prev_packed_read_b * 4 + current_packed_read_b];
    
    /* Update our rotation direction now */
    if(delta == -1)            _direction = Direction::CCW;
    else if (delta == 1)       _direction = Direction::CW;
    
    /* Update our local tracking variable */
    _counter += delta;
    
    /* Update our previous reading */
    _prev_packed_read_b = current_packed_read_b;

    /* Debug statistics */
    _channel_b_isr_cnt++;
}


void QuadratureEncoder::TrackGPIOPulseWidth(void)
{
    static std::atomic_int toggle;
    const auto now = std::chrono::high_resolution_clock::now();
    
    if(toggle++ % 2) {
        _isr_timestamp = now;
    } else {
        /* Calculate the pulsewidth on even transitions */
        auto delta = now - _isr_timestamp;
        _pulse_period_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(delta);
    }
}

