# G-Q-O-A Elastic Net selected linear models

## Variable normalization

- `g = G / 305143808`
- `q = Q / 192`
- `o = O / 32`
- `a = A / 128`

Every candidate term is a product of one, two, or three distinct variables.
Elastic Net with 5-fold CV and the one-standard-error rule selected terms.
Constituent main effects were retained for weak hierarchy, then coefficients
were refit by unregularized least squares on the selected terms.

## Forward / forward

**Formula (median milliseconds):**

`T = 0.55275919 + 63.472334·g - 0.67392917·q + 0.083254513·o + 1.1522232·a + 128.60883·go + 41.227363·oa - 179.37055·gqo - 9.3567713·gqa + 30.402194·qoa`

- Retained terms: G, Q, O, A, GO, OA, GQO, GQA, QOA
- Elastic Net direct terms: G, Q, O, A, GO, OA, GQO, GQA, QOA
- Elastic alpha (1-SE): 0.040433951
- Elastic l1 ratio: 0.95
- Nested-CV RMSE: 0.831425 ms
- Nested-CV MAE: 0.453079 ms
- Nested-CV R2: 0.989501

## Forward / total

**Formula (median milliseconds):**

`T = 0.55275919 + 63.472334·g - 0.67392917·q + 0.083254513·o + 1.1522232·a + 128.60883·go + 41.227363·oa - 179.37055·gqo - 9.3567713·gqa + 30.402194·qoa`

- Retained terms: G, Q, O, A, GO, OA, GQO, GQA, QOA
- Elastic Net direct terms: G, Q, O, A, GO, OA, GQO, GQA, QOA
- Elastic alpha (1-SE): 0.040433951
- Elastic l1 ratio: 0.95
- Nested-CV RMSE: 0.831425 ms
- Nested-CV MAE: 0.453079 ms
- Nested-CV R2: 0.989501

## ForwardPrepass / depth_prepass

**Formula (median milliseconds):**

`T = 0.39133167 + 92.666774·g - 0.52202382·q + 0.020475376·o - 0.026970711·a - 42.799421·gq - 0.40233657·go - 0.081196134·qo + 0.042168276·qa + 0.22964671·oa - 1.4827229·gqa`

- Retained terms: G, Q, O, A, GQ, GO, QO, QA, OA, GQA
- Elastic Net direct terms: G, Q, GQ, GO, QO, QA, OA, GQA
- Elastic alpha (1-SE): 0.009712884
- Elastic l1 ratio: 1
- Nested-CV RMSE: 0.651755 ms
- Nested-CV MAE: 0.362724 ms
- Nested-CV R2: 0.991158

## ForwardPrepass / forward

**Formula (median milliseconds):**

`T = 0.40316718 + 89.908002·g - 0.48212376·q - 0.0046776359·o + 1.2391092·a - 41.220072·gq + 0.47482486·go - 0.16916982·qo + 0.68128181·qa + 0.16387659·oa`

- Retained terms: G, Q, O, A, GQ, GO, QO, QA, OA
- Elastic Net direct terms: G, Q, A, GQ, GO, QO, QA, OA
- Elastic alpha (1-SE): 0.0096937451
- Elastic l1 ratio: 1
- Nested-CV RMSE: 0.672183 ms
- Nested-CV MAE: 0.368427 ms
- Nested-CV R2: 0.990592

## ForwardPrepass / total

**Formula (median milliseconds):**

`T = 0.78693593 + 182.56857·g - 0.94331558·q + 0.0067180686·o + 1.2505147·a - 84.531886·gq + 0.14399151·go - 0.21117204·qo + 0.49110866·qa + 0.3796194·oa`

- Retained terms: G, Q, O, A, GQ, GO, QO, QA, OA
- Elastic Net direct terms: G, Q, A, GQ, GO, QO, QA, OA
- Elastic alpha (1-SE): 0.019417118
- Elastic l1 ratio: 1
- Nested-CV RMSE: 1.30748 ms
- Nested-CV MAE: 0.721136 ms
- Nested-CV R2: 0.991102

## VisBuf / visibility

**Formula (median milliseconds):**

`T = 0.6090898 + 61.77499·g - 0.7665891·q + 0.40177916·o - 0.091277532·a + 5.9170779·go + 7.4511123·ga`

- Retained terms: G, Q, O, A, GO, GA
- Elastic Net direct terms: G, Q, O, GO, GA
- Elastic alpha (1-SE): 0.089603748
- Elastic l1 ratio: 1
- Nested-CV RMSE: 0.932191 ms
- Nested-CV MAE: 0.517557 ms
- Nested-CV R2: 0.968041

## VisBuf / resolve

**Formula (median milliseconds):**

`T = 0.029564133 + 0.0046297809·g - 0.00025458771·q - 0.0024587257·o + 1.2527511·a - 0.097951994·ga - 0.00036529014·qo`

- Retained terms: G, Q, O, A, GA, QO
- Elastic Net direct terms: O, A, GA, QO
- Elastic alpha (1-SE): 0.0017246018
- Elastic l1 ratio: 0.3
- Nested-CV RMSE: 0.00450689 ms
- Nested-CV MAE: 0.0036463 ms
- Nested-CV R2: 0.999867

## VisBuf / total

**Formula (median milliseconds):**

`T = 0.63882492 + 61.779708·g - 0.76699366·q + 0.39913034·o + 1.1615026·a + 5.91486·go + 7.3536213·ga`

- Retained terms: G, Q, O, A, GO, GA
- Elastic Net direct terms: G, Q, O, A, GO, GA
- Elastic alpha (1-SE): 0.049391613
- Elastic l1 ratio: 0.9
- Nested-CV RMSE: 0.933818 ms
- Nested-CV MAE: 0.512422 ms
- Nested-CV R2: 0.967927

## Data notes

- Source rows: 1000
- Successful rows used: 508
- Failed rows excluded: 492
- The uploaded CSV contains Forward, ForwardPrepass, and VisBuf.
  It does not contain a Deferred renderer.
- Forward `forward` and Forward `total` are exactly identical in this file.
