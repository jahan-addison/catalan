# The Basel problem

Find the exact value of

$$\sum_{n=1}^{\infty} \frac{1}{n^2} = 1 + \frac{1}{4} + \frac{1}{9} + \frac{1}{16} + \cdots$$

The partial sums converge slowly, since the tail after $N$ terms is about $\frac{1}{N}$, which is why the exact value resisted mathematicians for nearly a century.

## Euler's solution (1734)

The Taylor series of $\sin x$, divided by $x$, is

$$\frac{\sin x}{x} = 1 - \frac{x^2}{3!} + \frac{x^4}{5!} - \cdots$$

Its roots are $x = \pm n\pi$ for $n = 1, 2, 3, \ldots$, so Euler wrote it as a product over its roots, the way a polynomial factors:

$$\frac{\sin x}{x} = \prod_{n=1}^{\infty} \left(1 - \frac{x^2}{n^2 \pi^2}\right)$$

Expanding the product, the coefficient of $x^2$ is $-\sum \frac{1}{n^2 \pi^2}$. Matching it to the series' $-\frac{1}{3!}$:

$$\frac{1}{6} = \frac{1}{\pi^2} \sum_{n=1}^{\infty} \frac{1}{n^2}
\quad\Longrightarrow\quad
\sum_{n=1}^{\infty} \frac{1}{n^2} = \frac{\pi^2}{6}$$

> Euler's step from the series to the product wasn't rigorous until the **Weierstrass factorization theorem**, more than a century later.
