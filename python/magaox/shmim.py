from typing import Optional
import numpy as np
import ImageStreamIOWrap

class ShmimTimeout(Exception):
    pass

class ShapeMismatchException(Exception):
    pass

class Image(ImageStreamIOWrap.Image):
    semID : Optional[int] = None
    def __init__(self, name):
        super().__init__()
        ret = self.open(name)
        if ret != 0:
            raise RuntimeError(f"Could not open {repr(name)}")

    def copy(self):
        return np.squeeze(super().copy())

    def get_data(self, check: bool=False, timeout: Optional[float]=None):
        '''Get a copy of the image data, optionally waiting (with an
        optional timeout) for an updated frame to arrive before
        returning

        Parameters
        ----------
        check : bool
            Whether to block until the underlying shmim has an update
        timeout : float
            Number of seconds to wait before giving up
        '''
        if check:
            if self.semID is None:
                self.semID = self.getsemwaitindex(0)
                self.semflush(self.semID)
            if timeout is None:
                self.semwait(self.semID)
            else:
                if timeout < 0:
                    raise ValueError(f"Invalid timeout: {timeout}")
                ret = self.semtimedwait(self.semID, timeout)
                if ret != 0:
                    raise ShmimTimeout(f"Timed out after {timeout} sec without new data")
        return self.copy()
