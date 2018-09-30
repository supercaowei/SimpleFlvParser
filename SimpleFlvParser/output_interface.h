#ifndef _SFP_OUTPUT_INTERFACE_H_
#define _SFP_OUTPUT_INTERFACE_H_

#include <memory>

class FlvHeaderInterface;
class FlvTagInterface;
class NaluInterface;

class FlvOutputInterface
{
public:
	virtual ~FlvOutputInterface() = 0;
	virtual void FlvHeaderOutput(const std::shared_ptr<FlvHeaderInterface> header) = 0;
	virtual void FlvTagOutput(const std::shared_ptr<FlvTagInterface> tag) = 0;
	virtual void NaluOutput(const std::shared_ptr<NaluInterface> nalu) = 0;
};


#endif
