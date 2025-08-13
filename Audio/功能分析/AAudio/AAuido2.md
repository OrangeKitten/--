```mermaid
graph TD
    A[Application] --> B[API \(libaaudio/src/core\)]
    B --> C1[legacy \(libaaudio/src/legacy\)]
    B --> C2[client \(libaaudio/src/client\)]
    C2 --> D1[flowgraph \(libaaudio/src/fifo\)]
    C2 --> D2[fifo \(libaaudio/src/fifo\)]
    C2 --> D3[binding \(libaaudio/src/binding\)]
    D1 --> E[utility \(libaaudio/src/utility\)]
    D2 --> E
    D3 --> E
    B --> F[media.aaudio 服务]
```